/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
/**
*/
class RoomPannerLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
        float sliderPosProportional, float rotaryStartAngle,
        float rotaryEndAngle, juce::Slider& slider) override;
    void drawLabel(juce::Graphics& g, juce::Label& label) override;
};
class Roompan02AudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    Roompan02AudioProcessorEditor (Roompan02AudioProcessor&);
    ~Roompan02AudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void configureSlider(juce::Slider& slider, juce::Label& label, const juce::String& labelText);

    Roompan02AudioProcessor& audioProcessor;

    juce::Slider panSlider, widthSlider, ildSlider, depthSlider;
    juce::Label  panLabel, widthLabel, ildLabel, depthLabel;
    RoomPannerLookAndFeel roomPannerLookAndFeel;
    

    // Keeps each slider's position and value in sync with the matching
    // parameter, including host automation, with no manual wiring needed.
    juce::AudioProcessorValueTreeState::SliderAttachment panAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment widthAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment ildAttachment;
    juce::AudioProcessorValueTreeState::SliderAttachment depthAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Roompan02AudioProcessorEditor)
};
