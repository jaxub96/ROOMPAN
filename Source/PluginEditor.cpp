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
    addAndMakeVisible (content);
    content.addAndMakeVisible (sourcePositionView);
    configureSlider(panSlider, panLabel, "Pan");
    configureSlider(widthSlider, widthLabel, "Width");
    configureSlider(ildSlider, ildLabel, "ILD Amount");
    configureSlider(depthSlider, depthLabel, "Depth");
    setLookAndFeel (&customLookAndFeel);   // <-- this line was missing

    // Lock the aspect ratio so resizing always stays proportional
    setResizeLimits (300, 360, 1200, 1440);
    getConstrainer()->setFixedAspectRatio (baseWidth / baseHeight);
    setSize ((int) baseWidth, (int) baseHeight);
    setResizable (true, true);
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
    const float distance = processor.apvts.getRawParameterValue ("depth")->load();
    const float targetDistance01 = distance; // assuming already 0..1
    displayedDistance01 += (targetDistance01 - displayedDistance01) * 0.25f;

    repaint();
}

void SourcePositionView::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat().reduced (8.0f);
    const float radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();

    // replace these lines at the top of paint():
    const auto background = juce::Colour(RoomPanLookAndFeel::colourPanel);
    const auto track      = juce::Colour(0xffc2cbd6);
    const auto textColour = juce::Colour(RoomPanLookAndFeel::colourTextSecondary);
    const auto accent     = juce::Colour(0xff3a3d42);   // dark dot, matches knob body

    // fill the view's own background so it blends into the panel
    g.setColour(background);
    g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(4.0f), 8.0f);

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

        g.setColour(juce::Colour(0xff2e3238));
        g.fillEllipse(sx - 6.0f, sy - 6.0f, 12.0f, 12.0f);
        g.setColour(juce::Colours::white.withAlpha(0.6f));
        g.fillEllipse(sx - 2.0f, sy - 2.0f, 4.0f, 4.0f);
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
    setColour(juce::ResizableWindow::backgroundColourId,
              juce::Colour(colourBackground));
    setColour(juce::Slider::textBoxTextColourId,
              juce::Colour(colourTextPrimary));
    setColour(juce::Slider::textBoxBackgroundColourId,
              juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId,
              juce::Colours::transparentBlack);
}

void RoomPanLookAndFeel::drawRotarySlider(
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>((float)x, (float)y, (float)width, (float)height)
                      .reduced(8.0f);

    const float radius  = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    const float angle   = rotaryStartAngle
                        + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // --- Track ring background ---
    {
        const float trackR = radius * 0.92f;
        juce::Path trackArc;
        trackArc.addCentredArc(centreX, centreY, trackR, trackR,
                                0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colour(0xffc2cbd6));
        g.strokePath(trackArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // --- Value arc: white, same weight as track ---
    {
        const float trackR = radius * 0.92f;
        juce::Path valueArc;
        valueArc.addCentredArc(centreX, centreY, trackR, trackR,
                                0.0f, rotaryStartAngle, angle, true);
        g.setColour(juce::Colours::white);
        g.strokePath(valueArc, juce::PathStrokeType(3.5f, juce::PathStrokeType::curved,
                                                      juce::PathStrokeType::rounded));
    }

    // --- Knob shadow: soft dark ellipse slightly offset downward ---
    {
        auto shadowBounds = juce::Rectangle<float>(centreX - radius * 0.78f,
                                                    centreY - radius * 0.74f,
                                                    radius * 1.56f,
                                                    radius * 1.56f);
        g.setColour(juce::Colour(0x33000000));
        g.fillEllipse(shadowBounds.expanded(3.0f).translated(0.0f, 3.0f));
    }

    // --- Knob body: dark charcoal, slightly warm ---
    auto knobBounds = juce::Rectangle<float>(centreX - radius * 0.78f,
                                              centreY - radius * 0.78f,
                                              radius * 1.56f,
                                              radius * 1.56f);
    {
        juce::ColourGradient bodyGrad(
            juce::Colour(0xff3a3d42), centreX, knobBounds.getY(),
            juce::Colour(0xff26282c), centreX, knobBounds.getBottom(),
            false);
        g.setGradientFill(bodyGrad);
        g.fillEllipse(knobBounds);
    }

    // --- Subtle inner rim: slightly lighter edge at top ---
    g.setColour(juce::Colour(0xff4a4d52));
    g.drawEllipse(knobBounds.reduced(0.5f), 1.0f);

    // --- Pointer: short white line near edge ---
    {
        juce::Path pointer;
        const float innerR = radius * 0.45f;
        const float outerR = radius * 0.72f;
        pointer.startNewSubPath(0.0f, -innerR);
        pointer.lineTo(0.0f, -outerR);

        g.setColour(juce::Colours::white.withAlpha(0.9f));
        g.strokePath(pointer,
                     juce::PathStrokeType(2.2f, juce::PathStrokeType::curved,
                                           juce::PathStrokeType::rounded),
                     juce::AffineTransform().rotated(angle).translated(centreX, centreY));
    }

    // --- Value readout: clean number below knob, in a soft pill box ---
    if (auto* s = dynamic_cast<juce::Slider*>(&slider))
    {
        const auto valText = s->getTextFromValue(s->getValue());
        const float boxW   = 70.0f;
        const float boxH   = 22.0f;
        const float boxX   = centreX - boxW * 0.5f;
        const float boxY   = centreY + radius * 0.86f;

        juce::Rectangle<float> boxBounds(boxX, boxY, boxW, boxH);

        g.setColour(juce::Colour(0xffdce3ea));
        g.fillRoundedRectangle(boxBounds, 4.0f);

        g.setFont(juce::Font(juce::FontOptions(12.5f)).withStyle(juce::Font::plain));
        g.setColour(juce::Colour(0xff2e3238));
        g.drawText(valText, boxBounds, juce::Justification::centred);
    }
}
void RoomPanLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    auto bounds = label.getLocalBounds().toFloat();

    g.setFont(juce::Font(juce::FontOptions(12.0f)).withStyle(juce::Font::plain));
    g.setColour(juce::Colour(0xff4a5260));
    g.drawText(label.getText(), label.getLocalBounds().toFloat(),
               juce::Justification::centred);
}
void RoomPanAudioProcessorEditor::configureSlider(juce::Slider& slider, juce::Label& label,
                                                    const juce::String& labelText)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, true, 0, 0);
    content.addAndMakeVisible(slider);

    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    // label.attachToComponent removed — resized() handles positioning
    content.addAndMakeVisible(label);
}

void RoomPanAudioProcessorEditor::paint (juce::Graphics& g)
{
    const float headerH = 60.0f;

    g.fillAll (juce::Colour (RoomPanLookAndFeel::colourPanel));

    g.setColour (juce::Colour (RoomPanLookAndFeel::colourBackground));
    g.fillRect (0.0f, 0.0f, baseWidth, headerH);

    g.setFont (juce::Font (juce::FontOptions (22.0f)));
    g.setColour (juce::Colour (RoomPanLookAndFeel::colourTextPrimary));
    g.drawText ("roompan",
                juce::Rectangle<float> (16.0f, 10.0f, 200.0f, 28.0f),
                juce::Justification::centredLeft);

    g.setFont (juce::Font (juce::FontOptions (11.0f)));
    g.setColour (juce::Colour (RoomPanLookAndFeel::colourTextSecondary));
    g.drawText ("spatial panner  v0.1",
                juce::Rectangle<float> (16.0f, 38.0f, 260.0f, 16.0f),
                juce::Justification::centredLeft);

    g.setColour (juce::Colour (RoomPanLookAndFeel::colourTrack));
    g.drawLine (0.0f, headerH, baseWidth, headerH, 1.0f);
}

void RoomPanAudioProcessorEditor::resized()
{
    // Scale the entire UI uniformly from the base canvas size
    // to whatever the window currently is, maintaining aspect ratio
    const float scaleX = (float) getWidth()  / baseWidth;
    const float scaleY = (float) getHeight() / baseHeight;
    const float scale  = juce::jmin (scaleX, scaleY); // uniform — use the smaller axis

    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (0, 0,
                   (int) baseWidth,
                   (int) baseHeight);

    // All bounds are now in base-canvas coordinates — fixed, never recalculated
    const float headerH = 60.0f;
    const float margin  = 20.0f;
    const float gap     = 10.0f;

    auto area = juce::Rectangle<float> (0, 0, baseWidth, baseHeight);
    area.removeFromTop    (headerH);
    area.reduce           (margin, 0.0f);
    area.removeFromBottom (margin);

    content.addAndMakeVisible (sourcePositionView);
    sourcePositionView.setBounds (area.removeFromTop (180.0f).toNearestInt());
    area.removeFromTop (gap);

    const float labelH = 24.0f;
    const float colW   = area.getWidth() / 4.0f;

    auto labelRow = area.removeFromTop (labelH);
    auto knobRow  = area;

    panLabel.setBounds   (labelRow.removeFromLeft (colW).toNearestInt());
    widthLabel.setBounds (labelRow.removeFromLeft (colW).toNearestInt());
    ildLabel.setBounds   (labelRow.removeFromLeft (colW).toNearestInt());
    depthLabel.setBounds (labelRow.toNearestInt());

    const float knobPad = colW * 0.1f;
    panSlider.setBounds   (knobRow.removeFromLeft (colW).reduced (knobPad, 0).toNearestInt());
    widthSlider.setBounds (knobRow.removeFromLeft (colW).reduced (knobPad, 0).toNearestInt());
    ildSlider.setBounds   (knobRow.removeFromLeft (colW).reduced (knobPad, 0).toNearestInt());
    depthSlider.setBounds (knobRow.reduced        (knobPad, 0).toNearestInt());
}