#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

//==============================================================================
// RoomPanner
//==============================================================================
void RoomPanner::prepare(double newSampleRate, int /*maxBlockSize*/)
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

void RoomPanner::reset()
{
    std::fill(ringBuffer.begin(), ringBuffer.end(), 0.0f);
    writePos = 0;
}

void RoomPanner::setPan(float newPan) { smoothedPan.setTargetValue(newPan); }
void RoomPanner::setWidth(float newWidthPercent) { smoothedWidth.setTargetValue(newWidthPercent); }
void RoomPanner::setIldAmount(float newIldAmount) { smoothedIld.setTargetValue(newIldAmount); }
void RoomPanner::setDepth(float newDepth) { smoothedDepth.setTargetValue(newDepth); }

float RoomPanner::computeSignedItdSeconds(float panValue, float widthScale) const
{
    const auto theta = panValue * juce::MathConstants<float>::halfPi; // pan(-1..1) -> +/-90 degrees
    const auto magnitude = (headRadiusMetres / speedOfSoundMps)
        * (std::abs(theta) + std::sin(std::abs(theta)));
    return (panValue < 0.0f ? -magnitude : magnitude) * widthScale;
}

void RoomPanner::pushSample(float sample)
{
    ringBuffer[(size_t)writePos] = sample;
    writePos = (writePos + 1) % bufferSize;
}

float RoomPanner::readInterpolated(float delayInSamples) const
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

void RoomPanner::process(juce::AudioBuffer<float>& buffer)
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
// Roompan02AudioProcessor
//==============================================================================
Roompan02AudioProcessor::Roompan02AudioProcessor()
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

Roompan02AudioProcessor::~Roompan02AudioProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout Roompan02AudioProcessor::createParameterLayout()
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

void Roompan02AudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    roomPanner.prepare(sampleRate, samplesPerBlock);
    const auto latencySamples = (int)std::round(roomPanner.getLatencySeconds() * sampleRate);
    setLatencySamples(latencySamples);
}

void Roompan02AudioProcessor::releaseResources() {}

void Roompan02AudioProcessor::reset()
{
    roomPanner.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool Roompan02AudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
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

void Roompan02AudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    roomPanner.setPan(panParam->load());
    roomPanner.setWidth(widthParam->load());
    roomPanner.setIldAmount(ildParam->load());
    roomPanner.setDepth(depthParam->load());

    roomPanner.process(buffer);
}

juce::AudioProcessorEditor* Roompan02AudioProcessor::createEditor()
{
    return new Roompan02AudioProcessorEditor(*this);
}

bool Roompan02AudioProcessor::hasEditor() const { return true; }

const juce::String Roompan02AudioProcessor::getName() const { return JucePlugin_Name; }
bool Roompan02AudioProcessor::acceptsMidi() const { return false; }
bool Roompan02AudioProcessor::producesMidi() const { return false; }
bool Roompan02AudioProcessor::isMidiEffect() const { return false; }
double Roompan02AudioProcessor::getTailLengthSeconds() const { return 0.0; }

int Roompan02AudioProcessor::getNumPrograms() { return 1; }
int Roompan02AudioProcessor::getCurrentProgram() { return 0; }
void Roompan02AudioProcessor::setCurrentProgram(int) {}
const juce::String Roompan02AudioProcessor::getProgramName(int) { return {}; }
void Roompan02AudioProcessor::changeProgramName(int, const juce::String&) {}

void Roompan02AudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void Roompan02AudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState != nullptr && xmlState->hasTagName(apvts.state.getType()))
        apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
// This creates the instance of the plugin that the JUCE wrapper code talks to.
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new Roompan02AudioProcessor();
}