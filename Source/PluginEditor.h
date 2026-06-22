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

    static constexpr juce::uint32 colourBackground    = 0xfffaf8f5;
    static constexpr juce::uint32 colourTrack         = 0xffe8e4dd;
    static constexpr juce::uint32 colourTextPrimary   = 0xff2b2a28;
    static constexpr juce::uint32 colourTextSecondary = 0xff8c887f;
    static constexpr juce::uint32 colourAccent        = 0xff3e7c7c;
    static constexpr juce::uint32 colourAccentSoft    = 0xffa8c9c5;
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