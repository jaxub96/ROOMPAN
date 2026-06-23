#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
// RoomPanLookAndFeel
//==============================================================================

RoomPanLookAndFeel::RoomPanLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId,
               juce::Colour (theme.colourBackground));
    setColour (juce::Slider::textBoxTextColourId,
               juce::Colour (theme.colourTextPrimary));
    setColour (juce::Slider::textBoxBackgroundColourId,
               juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId,
               juce::Colours::transparentBlack);
}
void RoomPanLookAndFeel::setTheme (const ThemeConfig& newTheme)
{
    theme = newTheme;
    setColour (juce::ResizableWindow::backgroundColourId,
               juce::Colour (theme.colourBackground));
    setColour (juce::Slider::textBoxTextColourId,
               juce::Colour (theme.colourTextPrimary));
}
void RoomPanLookAndFeel::drawRotarySlider (
    juce::Graphics& g,
    int x, int y, int width, int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y,
                                           (float) width, (float) height)
                      .reduced (8.0f);

    const float radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();
    const float angle   = rotaryStartAngle
                        + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // --- Track ring background ---
    {
        const float trackR = radius * 0.92f;
        juce::Path trackArc;
        trackArc.addCentredArc (centreX, centreY, trackR, trackR,
                                 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (juce::Colour (theme.colourTrack));
        g.strokePath (trackArc, juce::PathStrokeType (3.5f,
                      juce::PathStrokeType::curved,
                      juce::PathStrokeType::rounded));
    }

    // --- Value arc ---
    {
        const float trackR = radius * 0.92f;
        juce::Path valueArc;
        valueArc.addCentredArc (centreX, centreY, trackR, trackR,
                                 0.0f, rotaryStartAngle, angle, true);
        g.setColour (juce::Colours::white);
        g.strokePath (valueArc, juce::PathStrokeType (3.5f,
                      juce::PathStrokeType::curved,
                      juce::PathStrokeType::rounded));
    }

    auto knobBounds = juce::Rectangle<float> (centreX - radius * 0.78f,
                                               centreY - radius * 0.78f,
                                               radius  * 1.56f,
                                               radius  * 1.56f);

    // --- 1. Drop shadow: drawn FIRST ---
    for (int i = 10; i > 0; --i)
    {
        const float expansion = (float) i * 0.3f;
        const float offsetY   = (float) i * 0.5f;
        const float offsetX   = (float) i * 0.1f;
        const float alpha     = (float) i * 0.01f;

        g.setColour (juce::Colours::black.withAlpha (alpha));
        g.fillEllipse (knobBounds.expanded (expansion).translated (offsetX, offsetY));
    }

    // --- 2. Knob body gradient ---
    // colourKnob is the top (lighter) colour, darken it for the bottom
    {
        const auto knobTop    = juce::Colour (theme.colourKnob);
        const auto knobBottom = knobTop.darker (0.4f); // derived, no extra token needed

        juce::ColourGradient bodyGrad (
            knobTop,    centreX, knobBounds.getY(),
            knobBottom, centreX, knobBounds.getBottom(),
            false);
        g.setGradientFill (bodyGrad);
        g.fillEllipse (knobBounds);
    }

    // --- 3. Specular glint ---
    {
        auto glintBounds = juce::Rectangle<float> (
            knobBounds.getX() + knobBounds.getWidth()  * 0.15f,
            knobBounds.getY() + knobBounds.getHeight() * 0.10f,
            knobBounds.getWidth()  * 0.38f,
            knobBounds.getHeight() * 0.25f);

        juce::ColourGradient glint (
            juce::Colours::white.withAlpha (0.12f),
            glintBounds.getCentreX(), glintBounds.getY(),
            juce::Colours::white.withAlpha (0.0f),
            glintBounds.getCentreX(), glintBounds.getBottom(),
            false);
        g.setGradientFill (glint);
        g.fillEllipse (glintBounds);
    }

    // --- 4. Pointer ---
    {
        juce::Path pointer;
        const float innerR = radius * 0.45f;
        const float outerR = radius * 0.72f;
        pointer.startNewSubPath (0.0f, -innerR);
        pointer.lineTo (0.0f, -outerR);

        g.setColour (juce::Colours::white.withAlpha (0.9f));
        g.strokePath (pointer,
                      juce::PathStrokeType (2.2f,
                          juce::PathStrokeType::curved,
                          juce::PathStrokeType::rounded),
                      juce::AffineTransform()
                          .rotated (angle)
                          .translated (centreX, centreY));
    }

    // --- 5. Value readout box ---
    if (auto* s = dynamic_cast<juce::Slider*> (&slider))
    {
        const auto valText = s->getTextFromValue (s->getValue());
        const float boxW   = 70.0f;
        const float boxH   = 22.0f;
        const float boxX   = centreX - boxW * 0.5f;
        const float boxY   = centreY + radius * 0.86f;

        juce::Rectangle<float> boxBounds (boxX, boxY, boxW, boxH);

        g.setColour (juce::Colour (theme.colourPanel));
        g.fillRoundedRectangle (boxBounds, 4.0f);

        g.setFont (juce::Font (juce::FontOptions (12.5f)));
        g.setColour (juce::Colour (theme.colourTextPrimary));
        g.drawText (valText, boxBounds, juce::Justification::centred);
    }
}

void RoomPanLookAndFeel::drawLabel (juce::Graphics& g, juce::Label& label)
{
    g.setFont (juce::Font (juce::FontOptions (theme.labelFontSize)));
    g.setColour (juce::Colour (theme.colourTextSecondary));
    g.drawText (label.getText(), label.getLocalBounds().toFloat(),
                juce::Justification::centred);
}

//==============================================================================
// SourcePositionView
//==============================================================================

SourcePositionView::SourcePositionView (RoomPanAudioProcessor& processorToTrack)
    : processor (processorToTrack)
{
    startTimerHz (30);
}

SourcePositionView::~SourcePositionView()
{
    stopTimer();
}

void SourcePositionView::setTheme (const ThemeConfig& newTheme)
{
    theme = newTheme;
    repaint();
}

void SourcePositionView::timerCallback()
{
    const float pan = processor.apvts.getRawParameterValue ("pan")->load();
    const float targetAngleRad = pan * juce::MathConstants<float>::halfPi;
    displayedAngleRad += (targetAngleRad - displayedAngleRad) * 0.25f;

    const float distance = processor.apvts.getRawParameterValue ("depth")->load();
    displayedDistance01 += (distance - displayedDistance01) * 0.25f;

    repaint();
}

void SourcePositionView::paint (juce::Graphics& g)
{
    // Uses its own stored ThemeConfig copy, updated via setTheme()
    auto bounds = getLocalBounds().toFloat().reduced (8.0f);
    const float radius  = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
    const float centreX = bounds.getCentreX();
    const float centreY = bounds.getCentreY();

    const auto background = juce::Colour (theme.colourPanel);
    const auto track      = juce::Colour (theme.colourTrack);
    const auto textColour = juce::Colour (theme.colourTextSecondary);
    const auto dotColour  = juce::Colour (theme.colourKnob);

    // Background
    g.setColour (background);
    g.fillRoundedRectangle (getLocalBounds().toFloat().reduced (4.0f), 8.0f);

    // Inset shadow effect around the edge of the view
    for (int i = 4; i > 0; --i)
    {
        g.setColour (juce::Colours::black.withAlpha (0.04f));
        g.drawRoundedRectangle (getLocalBounds().toFloat()
                                    .reduced (4.0f + (float)(4 - i) * 1.5f),
                                8.0f, 1.0f);
    }

    // Distance rings
    g.setColour (track);
    for (int i = 1; i <= 4; ++i)
    {
        const float r = radius * ((float) i / 4.0f);
        g.drawEllipse (centreX - r, centreY - r, r * 2.0f, r * 2.0f, 1.0f);
    }

    // Angle spokes
    for (int degrees = -90; degrees <= 90; degrees += 30)
    {
        const float rad = juce::degreesToRadians ((float) degrees);
        juce::Point<float> outer (centreX + radius * std::sin (rad),
                                   centreY - radius * std::cos (rad));
        g.setColour (track);
        g.drawLine (centreX, centreY, outer.x, outer.y, 1.0f);
    }

    // Listener marker
    {
        juce::Path listenerMark;
        const float s = 7.0f;
        listenerMark.addTriangle (centreX,           centreY - s,
                                   centreX - s * 0.7f, centreY + s * 0.5f,
                                   centreX + s * 0.7f, centreY + s * 0.5f);
        g.setColour (juce::Colour (theme.colourTextPrimary));
        g.fillPath (listenerMark);
    }

    // Source marker
    {
        const float sourceRadius = radius * displayedDistance01;
        const float sx = centreX + sourceRadius * std::sin (displayedAngleRad);
        const float sy = centreY - sourceRadius * std::cos (displayedAngleRad);

        g.setColour (dotColour.withAlpha (0.35f));
        g.drawLine (centreX, centreY, sx, sy, 1.2f);

        // Shadow under dot
        g.setColour (juce::Colours::black.withAlpha (0.2f));
        g.fillEllipse (sx - 6.0f, sy - 4.0f, 12.0f, 12.0f);

        g.setColour (dotColour);
        g.fillEllipse (sx - 6.0f, sy - 6.0f, 12.0f, 12.0f);

        g.setColour (juce::Colours::white.withAlpha (0.5f));
        g.fillEllipse (sx - 2.0f, sy - 2.0f, 4.0f, 4.0f);
    }

    // Outer boundary
    g.setColour (track);
    g.drawEllipse (centreX - radius, centreY - radius,
                   radius * 2.0f, radius * 2.0f, 1.4f);

    // L / R labels
    g.setFont (juce::Font (juce::FontOptions (11.0f)).withStyle (juce::Font::bold));
    g.setColour (textColour);
    g.drawText ("L", juce::Rectangle<float> (centreX - radius - 16, centreY - 8, 16, 16),
                juce::Justification::centred);
    g.drawText ("R", juce::Rectangle<float> (centreX + radius,      centreY - 8, 16, 16),
                juce::Justification::centred);
}

void SourcePositionView::resized() {}

//==============================================================================
// RoomPanAudioProcessorEditor
//==============================================================================

RoomPanAudioProcessorEditor::RoomPanAudioProcessorEditor (RoomPanAudioProcessor& p)
    : AudioProcessorEditor (&p),
      audioProcessor (p),
      sourcePositionView (p),
      panAttachment   (p.apvts, "pan",   panSlider),
      widthAttachment (p.apvts, "width", widthSlider),
      ildAttachment   (p.apvts, "ild",   ildSlider),
      depthAttachment (p.apvts, "depth", depthSlider)
{
    // Theme file lives next to PluginEditor.cpp on disk
    const auto sourceDir = juce::File (juce::String (__FILE__)).getParentDirectory();
    const auto themeFile = sourceDir.getChildFile ("theme.json");

    themeWatcher.onThemeChanged = [this] (const ThemeConfig& newTheme)
    {
        theme = newTheme;
        customLookAndFeel.setTheme (newTheme);
        sourcePositionView.setTheme (newTheme); // propagate to visualizer too
        content.repaint();

        repaint();
        resized();
    };

    themeWatcher.watch (themeFile);
    content.theme = &theme;
    // Add all children to content (scaled container)
    content.addAndMakeVisible (sourcePositionView);
    configureSlider (panSlider,   panLabel,   "Pan");
    configureSlider (widthSlider, widthLabel, "Width");
    configureSlider (ildSlider,   ildLabel,   "ILD Amount");
    configureSlider (depthSlider, depthLabel, "Depth");

    addAndMakeVisible (content);
    setLookAndFeel (&customLookAndFeel);

    setResizeLimits (300, 360, 1200, 1440);
    getConstrainer()->setFixedAspectRatio (baseWidth / baseHeight);
    setSize ((int) baseWidth, (int) baseHeight);
    setResizable (true, true);
}

RoomPanAudioProcessorEditor::~RoomPanAudioProcessorEditor()
{
    themeWatcher.stop();
    setLookAndFeel (nullptr);
}

void RoomPanAudioProcessorEditor::configureSlider (juce::Slider& slider,
                                                    juce::Label&  label,
                                                    const juce::String& labelText)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    content.addAndMakeVisible (slider);

    label.setText (labelText, juce::dontSendNotification);
    label.setJustificationType (juce::Justification::centred);
    content.addAndMakeVisible (label);
}

void RoomPanAudioProcessorEditor::paint (juce::Graphics& g)
{
    // Editor's own paint covers the area outside the scaled content
    // (letterbox bars if aspect ratio doesn't fill window exactly)
    g.fillAll (juce::Colour (theme.colourBackground));
}

void RoomPanAudioProcessorEditor::resized()
{
    const float scaleX = getWidth()  > 0 ? (float) getWidth()  / baseWidth  : 1.0f;
    const float scaleY = getHeight() > 0 ? (float) getHeight() / baseHeight : 1.0f;
    const float scale  = juce::jmax (0.01f, juce::jmin (scaleX, scaleY));

    const float scaledW = baseWidth  * scale;
    const float scaledH = baseHeight * scale;
    const int   offsetX = (int) ((getWidth()  - scaledW) * 0.5f);
    const int   offsetY = (int) ((getHeight() - scaledH) * 0.5f);

    content.setTransform (juce::AffineTransform::scale (scale));
    content.setBounds (offsetX, offsetY, (int) baseWidth, (int) baseHeight);

    // Layout in base-canvas coordinates
    const float headerH = theme.headerHeight;
    const float margin  = theme.margin;
    const float gap     = theme.gap;
    const float labelH  = theme.labelHeight;

    auto area = juce::Rectangle<float> (0, 0, baseWidth, baseHeight);
    area.removeFromTop    (headerH);   // skip header — content's paint() draws it
    area.reduce           (margin, 0.0f);
    area.removeFromBottom (margin);

    sourcePositionView.setBounds (area.removeFromTop (theme.vizHeight).toNearestInt());
    area.removeFromTop (gap);

    const float colW = area.getWidth() / 4.0f;
const float knobPad = colW * theme.knobPadding;

auto panCol   = area.removeFromLeft(colW);
auto widthCol = area.removeFromLeft(colW);
auto ildCol   = area.removeFromLeft(colW);
auto depthCol = area;

auto layoutColumn = [labelH, knobPad]
    (juce::Rectangle<float> col,
     juce::Component& label,
     juce::Component& slider)
{
    auto sliderBounds = col.reduced(knobPad, 0.0f);

    const float knobY = sliderBounds.getCentreY();

    auto labelBounds = juce::Rectangle<float>(
        sliderBounds.getX(),
        knobY - sliderBounds.getWidth() * 0.5f - labelH - 6.0f,
        sliderBounds.getWidth(),
        labelH);

    label.setBounds(labelBounds.toNearestInt());
    slider.setBounds(sliderBounds.toNearestInt());
};

layoutColumn(panCol,   panLabel,   panSlider);
layoutColumn(widthCol, widthLabel, widthSlider);
layoutColumn(ildCol,   ildLabel,   ildSlider);
layoutColumn(depthCol, depthLabel, depthSlider);
}