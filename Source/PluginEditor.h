#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

// ---- 1. LookAndFeel first, no dependencies ----
class RoomPanLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RoomPanLookAndFeel();

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPosProportional, float rotaryStartAngle,
                            float rotaryEndAngle, juce::Slider& slider) override;

    void drawLabel (juce::Graphics& g, juce::Label& label) override;
    static constexpr juce::uint32 colourBackground    = 0xffb8c4d0;  // cool blue-grey
    static constexpr juce::uint32 colourPanel         = 0xffc8d4de;  // slightly lighter panel
    static constexpr juce::uint32 colourTrack         = 0xffc2cbd6;  // arc track
    static constexpr juce::uint32 colourTextPrimary   = 0xff2e3238;  // near-black
    static constexpr juce::uint32 colourTextSecondary = 0xff4a5260;  // medium grey-blue
    static constexpr juce::uint32 colourAccent        = 0xffffffff;  // white — the arc
    static constexpr juce::uint32 colourKnob          = 0xff303338;  // dark knob body
};

// ---- 2. SourcePositionView second ----
// Must be declared before the editor, which holds it as a member.
// Must come after RoomPanLookAndFeel, whose colour tokens it uses in paint().
class SourcePositionView : public juce::Component, private juce::Timer
{
public:
    explicit SourcePositionView (RoomPanAudioProcessor& processorToTrack);
    ~SourcePositionView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    RoomPanAudioProcessor& processor;

    float displayedAngleRad   = 0.0f;
    float displayedDistance01 = 0.6f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourcePositionView)
};

// ---- 3. Editor last, can now use both types above as members ----
class RoomPanAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit RoomPanAudioProcessorEditor (RoomPanAudioProcessor&);
    ~RoomPanAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void configureSlider (juce::Slider& slider, juce::Label& label,
                          const juce::String& labelText);

    RoomPanAudioProcessor& audioProcessor;

    RoomPanLookAndFeel customLookAndFeel;   // must be before sliders
    SourcePositionView sourcePositionView;  // must be before attachments

    juce::Slider panSlider, widthSlider, ildSlider, depthSlider;
    juce::Label  panLabel,  widthLabel,  ildLabel,  depthLabel;

    juce::AudioProcessorValueTreeState::SliderAttachment panAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment widthAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment ildAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment depthAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (RoomPanAudioProcessorEditor)
};