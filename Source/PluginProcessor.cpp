#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
// RoomPan
//==============================================================================
void RoomPan::prepare(double newSampleRate, int /*maxBlockSize*/)
{
    sampleRate = newSampleRate;
    /* Woodworth-Schlosberg: 
    * Physical max ITD at 90 degrees away from center ITD = (r/c) * (theta + sin(theta))
    * r = head radius
    * c = speed of sound
    * theta = 90 = pi/2
    * sin(pi/2) = 1
    */
    constexpr float physicalMaxItdSeconds =
        (headRadiusMetres / speedOfSoundMps) * (juce::MathConstants<float>::halfPi + 1.0f); 

    /* width factor, 0% - 100% - 200% 
    controlled by Width-knob
    */
    const auto maxPossibleItdSeconds = physicalMaxItdSeconds * (maxWidthPercent / 100.0f);
    // calculate shift from center rather than L/R difference. 
    preDelaySeconds = maxPossibleItdSeconds * 0.5f;
    // calculate required samples. seconds -> samples + 8 samples of safety buffer.
    const auto requiredSamples = (int)std::ceil(maxPossibleItdSeconds * sampleRate) + 8;
    bufferSize = juce::nextPowerOfTwo(requiredSamples);

    ringBuffer.assign((size_t)bufferSize, 0.0f);
    writePos = 0;

    smoothedPan.reset(sampleRate, 0.05);
    smoothedWidth.reset(sampleRate, 0.05);
    smoothedIld.reset(sampleRate, 0.05);
    smoothedDepth.reset(sampleRate, 0.05);
}

void RoomPan::reset()
{
    std::fill(ringBuffer.begin(), ringBuffer.end(), 0.0f);
    writePos = 0;
}

void RoomPan::setPan(float newPan) { smoothedPan.setTargetValue(newPan); }
void RoomPan::setWidth(float newWidthPercent) { smoothedWidth.setTargetValue(newWidthPercent); }
void RoomPan::setIldAmount(float newIldAmount) { smoothedIld.setTargetValue(newIldAmount); }
void RoomPan::setDepth(float newDepth) { smoothedDepth.setTargetValue(newDepth); }

float RoomPan::computeSignedItdSeconds(float panValue, float widthScale) const
{
    const auto theta = panValue * juce::MathConstants<float>::halfPi; // pan(-1..1) -> +/-90 degrees
    const auto magnitude = (headRadiusMetres / speedOfSoundMps)
        * (std::abs(theta) + std::sin(std::abs(theta)));
    return (panValue < 0.0f ? -magnitude : magnitude) * widthScale;
}

void RoomPan::pushSample(float sample)
{
    ringBuffer[(size_t)writePos] = sample;
    writePos = (writePos + 1) % bufferSize;
}

float RoomPan::readInterpolated(float delayInSamples) const
{
    const auto bufSizeF = (float)bufferSize;
    auto readPos = (float)(writePos - 1) - delayInSamples;

    readPos = std::fmod(readPos, bufSizeF);
    if (readPos < 0.0f)
        readPos += bufSizeF;

    const auto index0 = (int)readPos;
    const auto frac = readPos - (float)index0;
    const auto index1 = (index0 + 1) % bufferSize;

    const auto s0 = ringBuffer[(size_t)index0];
    const auto s1 = ringBuffer[(size_t)index1];
    return s0 + frac * (s1 - s0);
}

void RoomPan::process(juce::AudioBuffer<float>& buffer)
{
    const auto numSamples = buffer.getNumSamples();
    const auto numChannels = buffer.getNumChannels();

    auto* left = buffer.getWritePointer(0);
    auto* right = numChannels > 1 ? buffer.getWritePointer(1) : nullptr;

    for (int n = 0; n < numSamples; ++n)
    {
        const auto pan = smoothedPan.getNextValue();
        const auto width = smoothedWidth.getNextValue();
        const auto ild = smoothedIld.getNextValue();
        const auto depth = smoothedDepth.getNextValue();

        // Collapse to a single source signal before spatializing it.
        const auto monoIn = right != nullptr ? 0.5f * (left[n] + right[n]) : left[n];
        pushSample(monoIn);

        const auto itdSeconds = computeSignedItdSeconds(pan, width / 100.0f);
        const auto itdSamples = itdSeconds * (float)sampleRate;
        const auto preDelaySamp = preDelaySeconds * (float)sampleRate;

        // Both delays stay non-negative across the whole pan range; their
        // difference is exactly the signed ITD computed above.
        const auto delayLeft = preDelaySamp + itdSamples * 0.5f;
        const auto delayRight = preDelaySamp - itdSamples * 0.5f;

        const auto tapLeft = readInterpolated(delayLeft);
        const auto tapRight = readInterpolated(delayRight);

        // Equal-power gain law, blended in by the ILD amount parameter.
        const auto angleNorm = (pan + 1.0f) * juce::MathConstants<float>::pi * 0.25f; // 0..pi/2
        const auto fullGainL = std::cos(angleNorm);
        const auto fullGainR = std::sin(angleNorm);

        const auto gainL = juce::jmap(ild, 1.0f, fullGainL);
        const auto gainR = juce::jmap(ild, 1.0f, fullGainR);
        
        const auto gainLD = juce::jmap(depth, gainL, 0.0f);
        const auto gainRD = juce::jmap(depth, gainR, 0.0f);

        left[n] = tapLeft * gainLD;
        if (right != nullptr)
            right[n] = tapRight * gainRD;
    }
}

//==============================================================================
// RoomPanAudioProcessor
//==============================================================================
RoomPanAudioProcessor::RoomPanAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
    apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    panParam = apvts.getRawParameterValue("pan");
    widthParam = apvts.getRawParameterValue("width");
    ildParam = apvts.getRawParameterValue("ild");
    depthParam = apvts.getRawParameterValue("depth");
}

RoomPanAudioProcessor::~RoomPanAudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout RoomPanAudioProcessor::createParameterLayout()
{
    // Note: some JUCE versions (7.x+) expect juce::ParameterID ("pan", 1) as the
    // first constructor argument instead of a bare string. If this doesn't
    // compile on your version, wrap each ID that way.
    return {
        std::make_unique<juce::AudioParameterFloat>(
            "pan", "Pan",
            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.001f), 0.0f),

        std::make_unique<juce::AudioParameterFloat>(
            "width", "Width",
            juce::NormalisableRange<float>(0.0f, 200.0f, 0.1f), 100.0f),

        std::make_unique<juce::AudioParameterFloat>(
            "ild", "ILD Amount",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f),

        std::make_unique<juce::AudioParameterFloat>(
            "depth", "Depth Amount",
            juce::NormalisableRange<float>(0.0f, 1.0f, 0.01f), 0.5f)

    };
}

void RoomPanAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    roomPan.prepare(sampleRate, samplesPerBlock);
    const auto newLatencySamples = (int)std::round(roomPan.getLatencySeconds() * sampleRate);
    setLatencySamples(newLatencySamples);
}

void RoomPanAudioProcessor::releaseResources() {}

void RoomPanAudioProcessor::reset()
{
    roomPan.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool RoomPanAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}
#endif

void RoomPanAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    roomPan.setPan(panParam->load());
    roomPan.setWidth(widthParam->load());
    roomPan.setIldAmount(ildParam->load());
    roomPan.setDepth(depthParam->load());

    roomPan.process(buffer);
}

juce::AudioProcessorEditor* RoomPanAudioProcessor::createEditor()
{
    return new RoomPanAudioProcessorEditor(*this);
}

bool RoomPanAudioProcessor::hasEditor() const { return true; }

const juce::String RoomPanAudioProcessor::getName() const { return JucePlugin_Name; }
bool RoomPanAudioProcessor::acceptsMidi() const { return false; }
bool RoomPanAudioProcessor::producesMidi() const { return false; }
bool RoomPanAudioProcessor::isMidiEffect() const { return false; }
double RoomPanAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int RoomPanAudioProcessor::getNumPrograms() { return 1; }
int RoomPanAudioProcessor::getCurrentProgram() { return 0; }
void RoomPanAudioProcessor::setCurrentProgram(int) {}
const juce::String RoomPanAudioProcessor::getProgramName(int) { return {}; }
void RoomPanAudioProcessor::changeProgramName(int, const juce::String&) {}

void RoomPanAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void RoomPanAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
// This creates the instance of the plugin that the JUCE wrapper code talks to.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new RoomPanAudioProcessor();
}