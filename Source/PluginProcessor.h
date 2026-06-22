#pragma once

#include <JuceHeader.h>

//==============================================================================
// ITD BASED spatial panner.
//
// Instead of only changing left/right gain, this also delays one channel
// relative to the other (interaural time difference, ITD), based on the
// Woodworth-Schlosberg spherical head model, so the pan feels like a real
// source position rather than just a volume balance.
class RoomPan
{
public:
    RoomPan() = default;

    void prepare(double newSampleRate, int maxBlockSize);
    void reset();

    // Processes buffer in place. Reads channel 0 (and channel 1, if present,
    // averaged into a mono source) and writes a full stereo image back into
    // channels 0 and 1.
    void process(juce::AudioBuffer<float>& buffer);

    void setPan(float newPan);               // -1 (left) .. +1 (right)
    void setWidth(float newWidthPercent);    // 0 .. 200 (%), 100 = natural max ITD
    void setIldAmount(float newIldAmount);   // 0 .. 1, blends in the equal-power gain law
    void setDepth(float newDepth);            // 0 .. 1, distance of sound source
    float getLatencySeconds() const { return preDelaySeconds; }

private:
    float computeSignedItdSeconds(float panValue, float widthScale) const;
    void pushSample(float sample);
    float readInterpolated(float delayInSamples) const;

    double sampleRate = 44100.0;

    std::vector<float> ringBuffer;
    int writePos = 0;
    int bufferSize = 0;

    float preDelaySeconds = 0.0f;

    juce::SmoothedValue<float> smoothedPan{ 0.0f };
    juce::SmoothedValue<float> smoothedWidth{ 100.0f };
    juce::SmoothedValue<float> smoothedIld{ 0.5f };
    juce::SmoothedValue<float> smoothedDepth{ 0.5f };

    static constexpr float headRadiusMetres = 0.0875f;   // ~average adult head radius
    static constexpr float speedOfSoundMps = 343.0f;
    static constexpr float maxWidthPercent = 200.0f;     // upper bound used to size the buffer
};

//==============================================================================
class RoomPanAudioProcessor : public juce::AudioProcessor
{
public:
    RoomPanAudioProcessor();
    ~RoomPanAudioProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void reset() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Exposed so the editor can attach sliders directly to parameters.
    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    RoomPan roomPan;

    std::atomic<float>* panParam = nullptr;
    std::atomic<float>* widthParam = nullptr;
    std::atomic<float>* ildParam = nullptr;
    std::atomic<float>* depthParam = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(RoomPanAudioProcessor)
};