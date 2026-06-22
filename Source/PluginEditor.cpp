/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================

RoomPanAudioProcessorEditor::RoomPanAudioProcessorEditor (RoomPanAudioProcessor& p)
    : AudioProcessorEditor (&p), 
    audioProcessor (p),
    sourcePositionView (p),
    panAttachment(p.apvts, "pan", panSlider),
    widthAttachment(p.apvts, "width", widthSlider),
    ildAttachment(p.apvts, "ild", ildSlider),
    depthAttachment(p.apvts, "depth", depthSlider)
{
    addAndMakeVisible (sourcePositionView);
    configureSlider(panSlider, panLabel, "Pan");
    configureSlider(widthSlider, widthLabel, "Width");
    configureSlider(ildSlider, ildLabel, "ILD Amount");
    configureSlider(depthSlider, depthLabel, "Depth");
    setLookAndFeel (&customLookAndFeel);   // <-- this line was missing

    setSize(600, 400);
    setResizable(true, true);
}

RoomPanAudioProcessorEditor::~RoomPanAudioProcessorEditor()
{
    setLookAndFeel (nullptr);
    panSlider.setLookAndFeel(nullptr);
    widthSlider.setLookAndFeel(nullptr);
    ildSlider.setLookAndFeel(nullptr);
    depthSlider.setLookAndFeel(nullptr);
}
// ============================================================
// PluginEditor.cpp — SourcePositionView implementation
// ============================================================


SourcePositionView::SourcePositionView (RoomPanAudioProcessor& processorToTrack)
    : processor (processorToTrack)
{
    startTimerHz (30);
}

SourcePositionView::~SourcePositionView()
{
    stopTimer();
}

void SourcePositionView::timerCallback()
{
    // pan: -1..+1 -> angle: -90..+90 degrees, matching the azimuth mapping
    // used in RoomPanner::computeSignedItdSeconds
    const float pan = processor.apvts.getRawParameterValue ("pan")->load();
    const float targetAngleRad = pan * juce::MathConstants<float>::halfPi;

    // gentle smoothing toward the target so the dot glides rather than snaps
    displayedAngleRad += (targetAngleRad - displayedAngleRad) * 0.25f;

    // placeholder: once a "distance" parameter exists, read and map it here
    // const float distance = processor.apvts.getRawParameterValue ("distance")->load();
    // const float targetDistance01 = distance; // assuming already 0..1
    // displayedDistance01 += (targetDistance01 - displayedDistance01) * 0.25f;

    repaint();
}

void SourcePositionView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (8.0f);
    const float radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();

    const auto track      = juce::Colour (RoomPanLookAndFeel::colourTrack);
    const auto textColour = juce::Colour (RoomPanLookAndFeel::colourTextSecondary);
    const auto accent     = juce::Colour (RoomPanLookAndFeel::colourAccent);

    // --- Distance rings: concentric circles at 25/50/75/100% radius ---
    g.setColour (track);
    for (int i = 1; i <= 4; ++i)
    {
        const float r = radius * ((float) i / 4.0f);
        g.drawEllipse (centreX - r, centreY - r, r * 2.0f, r * 2.0f, 1.0f);
    }

    // --- Angle spokes: every 30 degrees across the front 180-degree field ---
    for (int degrees = -90; degrees <= 90; degrees += 30)
    {
        const float rad = juce::degreesToRadians ((float) degrees);
        juce::Point<float> outer (centreX + radius * std::sin (rad),
                                   centreY - radius * std::cos (rad));
        g.drawLine (centreX, centreY, outer.x, outer.y, 1.0f);
    }

    // --- Listener marker at origin: small triangle pointing "forward" (up) ---
    {
        juce::Path listenerMark;
        const float s = 7.0f;
        listenerMark.addTriangle (centreX, centreY - s,
                                   centreX - s * 0.7f, centreY + s * 0.5f,
                                   centreX + s * 0.7f, centreY + s * 0.5f);
        g.setColour (juce::Colour (RoomPanLookAndFeel::colourTextPrimary));
        g.fillPath (listenerMark);
    }

    // --- Source marker: the one accent-coloured element, derived from pan/distance ---
    {
        const float sourceRadius = radius * displayedDistance01;
        const float sx = centreX + sourceRadius * std::sin (displayedAngleRad);
        const float sy = centreY - sourceRadius * std::cos (displayedAngleRad);

        // thin connecting line from origin to source, reinforcing the polar read
        g.setColour (accent.withAlpha (0.35f));
        g.drawLine (centreX, centreY, sx, sy, 1.2f);

        g.setColour (accent);
        g.fillEllipse (sx - 5.0f, sy - 5.0f, 10.0f, 10.0f);
        g.setColour (juce::Colours::white);
        g.fillEllipse (sx - 1.8f, sy - 1.8f, 3.6f, 3.6f); // small centre highlight
    }

    // --- Outer boundary circle ---
    g.setColour (track);
    g.drawEllipse (centreX - radius, centreY - radius, radius * 2.0f, radius * 2.0f, 1.4f);

    // --- "L" / "R" labels at the horizontal extremes ---
    g.setFont (juce::Font (juce::FontOptions (11.0f)).withStyle (juce::Font::bold));
    g.setColour (textColour);
    g.drawText ("L", juce::Rectangle<float> (centreX - radius - 16, centreY - 8, 16, 16),
                juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (centreX + radius,      centreY - 8, 16, 16),
                juce::Justification::centred);
}

void SourcePositionView::resized()
{
    // nothing to lay out internally — paint() reads getLocalBounds() directly
}
RoomPanLookAndFeel::RoomPanLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, juce::Colour (colourBackground));
    setColour (juce::Slider::textBoxTextColourId,         juce::Colour (colourTextPrimary));
    setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
}

void RoomPanLookAndFeel::drawRotarySlider(
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                      .reduced (6.0f);

    auto radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();

    auto angle = rotaryStartAngle
        + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    const auto accent     = juce::Colour (colourAccent);
    const auto accentSoft = juce::Colour (colourAccentSoft);
    const auto track      = juce::Colour (colourTrack);
    const auto textColour = juce::Colour (colourTextSecondary);

    // --- Tick scale: thin, precise marks, 0 to 10, no filled wedge ---
    constexpr int numTicks = 11;
    const float tickRadius      = radius * 1.0f;
    const float tickLengthMinor = radius * 0.09f;
    const float tickLengthMajor = radius * 0.16f;

    for (int i = 0; i < numTicks; ++i)
    {
        const float proportion = (float) i / (float) (numTicks - 1);
        const float tickAngle  = rotaryStartAngle
            + proportion * (rotaryEndAngle - rotaryStartAngle);

        const bool isMajor  = (i % 5 == 0);
        const bool isActive = proportion <= sliderPosProportional + 0.001f;
        const float tickLength = isMajor ? tickLengthMajor : tickLengthMinor;

        juce::Point<float> inner (centreX + (tickRadius - tickLength) * std::sin (tickAngle),
                                   centreY - (tickRadius - tickLength) * std::cos (tickAngle));
        juce::Point<float> outer (centreX + tickRadius * std::sin (tickAngle),
                                   centreY - tickRadius * std::cos (tickAngle));

        g.setColour (isActive ? accent : track);
        g.drawLine ({ inner, outer }, isMajor ? 1.6f : 1.0f);
    }

    // --- Knob body: flat, single soft fill, no bevel or gradient ---
    auto knobBounds = juce::Rectangle<float> (centreX - radius * 0.66f, centreY - radius * 0.66f,
                                               radius * 1.32f, radius * 1.32f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (knobBounds);
    g.setColour (track);
    g.drawEllipse (knobBounds, 1.2f);

    // --- Pointer: a single clean line from centre toward the edge ---
    {
        juce::Path pointer;
        const float innerR = radius * 0.18f;
        const float outerR = radius * 0.58f;
        pointer.startNewSubPath (0.0f, -innerR);
        pointer.lineTo (0.0f, -outerR);

        g.setColour (accent);
        g.strokePath (pointer, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                       juce::PathStrokeType::rounded),
                      juce::AffineTransform().rotated (angle).translated (centreX, centreY));

        // small centre dot
        g.setColour (accent);
        g.fillEllipse (centreX - 2.5f, centreY - 2.5f, 5.0f, 5.0f);
    }

    // --- Live value readout, beneath the knob, in place of a boxed textbox ---
    if (auto* s = dynamic_cast<juce::Slider*> (&slider))
    {
        g.setFont (juce::Font (juce::FontOptions (12.0f)));
        g.setColour (textColour);
        g.drawText (s->getTextFromValue (s->getValue()),
                    juce::Rectangle<float> (centreX - 40, centreY + radius * 0.78f, 80, 16),
                    juce::Justification::centred);
    }
}

void RoomPanLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    auto bounds = label.getLocalBounds().toFloat();

    g.setFont (juce::Font (juce::FontOptions (13.0f)).withStyle (juce::Font::bold));
    g.setColour (juce::Colour (colourTextPrimary));
    g.drawText (label.getText().toUpperCase(), bounds, juce::Justification::centred);
}
void RoomPanAudioProcessorEditor::configureSlider (juce::Slider& slider, juce::Label& label,
                                                     const juce::String& labelText)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0); // custom readout drawn in LookAndFeel
    addAndMakeVisible (slider);

    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (13.0f)).withStyle (juce::Font::bold));
    label.attachToComponent (&slider, false);
    addAndMakeVisible (label);
}

void RoomPanAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (RoomPanLookAndFeel::colourBackground));

    // a single thin hairline beneath the title area — the one quiet structural accent
    g.setColour (juce::Colour (RoomPanLookAndFeel::colourTrack));
    g.drawLine (20.0f, 56.0f, (float) getWidth() - 20.0f, 56.0f, 1.0f);

    g.setColour (juce::Colour (RoomPanLookAndFeel::colourTextPrimary));
    g.setFont (juce::Font (juce::FontOptions (16.0f)).withStyle (juce::Font::bold));
    g.drawText ("Room Panner", juce::Rectangle<int> (0, 16, getWidth(), 24),
                juce::Justification::centred);
}



void RoomPanAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(20); // leave room for the labels attached above each slider
    const auto sliderWidth = area.getWidth() / 4;
    sourcePositionView.setBounds (area.removeFromTop (140));
    panSlider.setBounds(area.removeFromLeft(sliderWidth).reduced(10));
    widthSlider.setBounds(area.removeFromLeft(sliderWidth).reduced(10));
    ildSlider.setBounds(area.removeFromLeft(sliderWidth).reduced(10));
    depthSlider.setBounds(area.reduced(10));
}

