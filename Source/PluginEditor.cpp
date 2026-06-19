/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================

Roompan02AudioProcessorEditor::Roompan02AudioProcessorEditor (Roompan02AudioProcessor& p)
    : AudioProcessorEditor (&p), 
    audioProcessor (p),
    panAttachment(p.apvts, "pan", panSlider),
    widthAttachment(p.apvts, "width", widthSlider),
    ildAttachment(p.apvts, "ild", ildSlider),
    depthAttachment(p.apvts, "depth", depthSlider)
{

    configureSlider(panSlider, panLabel, "Pan");
    configureSlider(widthSlider, widthLabel, "Width");
    configureSlider(ildSlider, ildLabel, "ILD Amount");
    configureSlider(depthSlider, depthLabel, "Depth");

    setSize(420, 220);
    setResizable(true, true);
}

Roompan02AudioProcessorEditor::~Roompan02AudioProcessorEditor()
{
    panSlider.setLookAndFeel(nullptr);
    widthSlider.setLookAndFeel(nullptr);
    ildSlider.setLookAndFeel(nullptr);
    depthSlider.setLookAndFeel(nullptr);
}
void RoomPannerLookAndFeel::drawLabel(juce::Graphics& g, juce::Label& label)
{
    auto bounds = label.getLocalBounds().toFloat();

    g.setFont(juce::Font(juce::FontOptions(13.0f)).withStyle(juce::Font::bold));

    // faint dark "engraved" shadow offset down-right
    g.setColour(juce::Colours::black.withAlpha(0.5f));
    g.drawText(label.getText(), bounds.translated(0.5f, 0.8f),
        juce::Justification::centred);

    // bright "highlight" offset up-left, like light catching a debossed edge
    g.setColour(juce::Colours::white.withAlpha(0.25f));
    g.drawText(label.getText(), bounds.translated(-0.5f, -0.5f),
        juce::Justification::centred);

    // main text, slightly desaturated off-white rather than pure digital black/white
    g.setColour(juce::Colour(0xffd8d4c8));
    g.drawText(label.getText(), bounds, juce::Justification::centred);
}
void RoomPannerLookAndFeel::drawRotarySlider(
    juce::Graphics& g,
    int x,
    int y,
    int width,
    int height,
    float sliderPosProportional,
    float rotaryStartAngle,
    float rotaryEndAngle,
    juce::Slider& slider)
{
    auto bounds = juce::Rectangle<float>(
        (float)x, (float)y, (float)width, (float)height).reduced(4.0f);

    auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) * 0.5f;
    auto centreX = bounds.getCentreX();
    auto centreY = bounds.getCentreY();

    auto angle = rotaryStartAngle
        + sliderPosProportional * (rotaryEndAngle - rotaryStartAngle);

    // --- Outer ring: a thin, dark recessed groove the knob sits inside ---
    auto outerBounds = juce::Rectangle<float>(centreX - radius, centreY - radius,
        radius * 2.0f, radius * 2.0f);
    g.setColour(juce::Colours::black.withAlpha(0.6f));
    g.drawEllipse(outerBounds, 1.5f);

    // --- Value arc: shows current position around the knob, like a track ---
    // --- Tick-mark scale: fixed marks like a 1-10 printed amp scale ---
    {
        constexpr int numTicks = 11; // 0 through 10
        const float tickRadius = radius * 1.05f;
        const float tickLengthMinor = radius * 0.10f;
        const float tickLengthMajor = radius * 0.16f;

        for (int i = 0; i < numTicks; ++i)
        {
            const float tickProportion = (float)i / (float)(numTicks - 1);
            const float tickAngle = rotaryStartAngle
                + tickProportion * (rotaryEndAngle - rotaryStartAngle);

            // every 5th tick (0, 5, 10) is drawn longer/brighter, like printed major marks
            const bool isMajor = (i % 5 == 0);
            const float tickLength = isMajor ? tickLengthMajor : tickLengthMinor;

            const bool isActive = tickProportion <= sliderPosProportional + 0.001f;

            juce::Point<float> inner(centreX + (tickRadius - tickLength) * std::sin(tickAngle),
                centreY - (tickRadius - tickLength) * std::cos(tickAngle));
            juce::Point<float> outer(centreX + tickRadius * std::sin(tickAngle),
                centreY - tickRadius * std::cos(tickAngle));

            g.setColour(isActive ? juce::Colours::silver
                : juce::Colours::black.withAlpha(0.35f));
            g.drawLine({ inner, outer }, isMajor ? 2.0f : 1.2f);
        }
        // numeral labels at major ticks only (0, 5, 10 in this 11-tick layout)
        g.setFont(juce::Font(juce::FontOptions(10.0f)).withStyle(juce::Font::bold));
        g.setColour(juce::Colours::black.withAlpha(0.55f));

        for (int i = 0; i <= 10; i += 5)
        {
            const float tickProportion = (float)i / 10.0f;
            const float tickAngle = rotaryStartAngle
                + tickProportion * (rotaryEndAngle - rotaryStartAngle);

            const float labelRadius = radius * 1.22f;
            juce::Point<float> labelPos(centreX + labelRadius * std::sin(tickAngle),
                centreY - labelRadius * std::cos(tickAngle));

            g.drawText(juce::String(i / 1 == 0 ? 0 : i), // simple int label, see note below
                juce::Rectangle<float>(labelPos.x - 10, labelPos.y - 7, 20, 14),
                juce::Justification::centred);
        }
    }

    auto knobBounds = juce::Rectangle<float>(centreX - radius * 0.78f, centreY - radius * 0.78f,
        radius * 1.56f, radius * 1.56f);

    // --- Knob body: radial gradient gives the curved-metal look ---
    {
        juce::ColourGradient bodyGradient(
            juce::Colour(0xffe8e8ea), centreX - radius * 0.3f, centreY - radius * 0.3f,
            juce::Colour(0xff6b6b70), centreX, centreY,
            true); // radial
        bodyGradient.addColour(0.55, juce::Colour(0xffb8b8bd));
        bodyGradient.addColour(0.85, juce::Colour(0xff86868c));

        g.setGradientFill(bodyGradient);
        g.fillEllipse(knobBounds);
    }

    // --- Bevel: a darker ring at the very edge sells material thickness ---
    g.setColour(juce::Colours::black.withAlpha(0.35f));
    g.drawEllipse(knobBounds, 1.0f);

    // --- Specular highlight: small bright patch offset toward upper-left ---
    {
        auto highlightBounds = juce::Rectangle<float>(
            knobBounds.getX() + knobBounds.getWidth() * 0.18f,
            knobBounds.getY() + knobBounds.getHeight() * 0.14f,
            knobBounds.getWidth() * 0.42f,
            knobBounds.getHeight() * 0.30f);

        juce::ColourGradient highlightGradient(
            juce::Colours::white.withAlpha(0.85f), highlightBounds.getCentreX(), highlightBounds.getY(),
            juce::Colours::white.withAlpha(0.0f), highlightBounds.getCentreX(), highlightBounds.getBottom(),
            false);

        g.setGradientFill(highlightGradient);
        g.fillEllipse(highlightBounds);
    }

    // --- Pointer: a notch cut into the metal showing the angle ---
    {
        juce::Path pointer;
        const float pointerLength = radius * 0.62f;
        const float pointerWidth = 3.0f;
        pointer.addRoundedRectangle(-pointerWidth * 0.5f, -pointerLength,
            pointerWidth, pointerLength * 0.85f, 1.5f);

        g.setColour(juce::Colours::black.withAlpha(0.75f));
        g.fillPath(pointer, juce::AffineTransform()
            .rotated(angle)
            .translated(centreX, centreY));

        // thin bright sliver alongside the groove, like light catching the cut edge
        juce::Path pointerHighlight;
        pointerHighlight.addRoundedRectangle(-pointerWidth * 0.5f + 0.6f, -pointerLength,
            pointerWidth * 0.3f, pointerLength * 0.8f, 1.0f);
        g.setColour(juce::Colours::white.withAlpha(0.4f));
        g.fillPath(pointerHighlight, juce::AffineTransform()
            .rotated(angle)
            .translated(centreX, centreY));
    }
}
void Roompan02AudioProcessorEditor::configureSlider(juce::Slider& slider, juce::Label& label,
    const juce::String& labelText)
{
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 70, 20);

    addAndMakeVisible(slider);
    slider.setLookAndFeel(&roomPannerLookAndFeel);
    label.setText(labelText, juce::dontSendNotification);
    label.setJustificationType(juce::Justification::centred);
    label.attachToComponent(&slider, false);
    addAndMakeVisible(label);
    
}
//==============================================================================
void Roompan02AudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void Roompan02AudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(20); // leave room for the labels attached above each slider
    const auto sliderWidth = area.getWidth() / 4;

    panSlider.setBounds(area.removeFromLeft(sliderWidth).reduced(10));
    widthSlider.setBounds(area.removeFromLeft(sliderWidth).reduced(10));
    ildSlider.setBounds(area.removeFromLeft(sliderWidth).reduced(10));
    depthSlider.setBounds(area.reduced(10));
}

