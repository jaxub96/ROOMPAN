#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

struct ThemeConfig
{
    // Colours
    juce::uint32 colourBackground    = 0xffb8c4d0;
    juce::uint32 colourPanel         = 0xffc8d4de;
    juce::uint32 colourTrack         = 0xffc2cbd6;
    juce::uint32 colourTextPrimary   = 0xff2e3238;
    juce::uint32 colourTextSecondary = 0xff4a5260;
    juce::uint32 colourAccent        = 0xffffffff;
    juce::uint32 colourKnob          = 0xff303338;

    // Layout
    float headerHeight   = 60.0f;
    float margin         = 20.0f;
    float labelHeight    = 24.0f;
    float knobPadding    = 0.1f;
    float vizHeight      = 180.0f;
    float gap            = 10.0f;

    // Typography
    float titleFontSize    = 22.0f;
    float subtitleFontSize = 11.0f;
    float labelFontSize    = 12.0f;
    float valueFontSize    = 12.5f;
};
class ThemeWatcher : private juce::Timer
{
public:
    // Callback fires whenever the file changes on disk
    std::function<void(const ThemeConfig&)> onThemeChanged;

    void watch (const juce::File& fileToWatch)
    {
        themeFile    = fileToWatch;
        lastModTime  = themeFile.getLastModificationTime();
        loadAndNotify();   // load immediately on startup
        startTimerHz (4);  // poll 4 times per second — fast enough to feel instant
    }

    void stop() { stopTimer(); }

private:
    void timerCallback() override
    {
        if (! themeFile.existsAsFile())
            return;

        const auto modTime = themeFile.getLastModificationTime();
        if (modTime != lastModTime)
        {
            lastModTime = modTime;
            loadAndNotify();
        }
    }

    void loadAndNotify()
    {
        const auto config = parseFile (themeFile);
        if (onThemeChanged)
            onThemeChanged (config);
    }

    ThemeConfig parseFile (const juce::File& file)
    {
        ThemeConfig config; // starts with defaults

        if (! file.existsAsFile())
            return config;

        const auto jsonText = file.loadFileAsString();
        auto parsed = juce::JSON::parse (jsonText);

        if (auto* obj = parsed.getDynamicObject())
        {
            auto readColour = [&](const char* key, juce::uint32& target)
            {
                if (obj->hasProperty (key))
                    target = (juce::uint32) obj->getProperty (key).toString()
                                 .getHexValue32();
            };

            auto readFloat = [&](const char* key, float& target)
            {
                if (obj->hasProperty (key))
                    target = (float) obj->getProperty (key);
            };

            readColour ("colourBackground",    config.colourBackground);
            readColour ("colourPanel",         config.colourPanel);
            readColour ("colourTrack",         config.colourTrack);
            readColour ("colourTextPrimary",   config.colourTextPrimary);
            readColour ("colourTextSecondary", config.colourTextSecondary);
            readColour ("colourAccent",        config.colourAccent);
            readColour ("colourKnob",          config.colourKnob);

            readFloat ("headerHeight",   config.headerHeight);
            readFloat ("margin",         config.margin);
            readFloat ("labelHeight",    config.labelHeight);
            readFloat ("knobPadding",    config.knobPadding);
            readFloat ("vizHeight",      config.vizHeight);
            readFloat ("gap",            config.gap);
            readFloat ("titleFontSize",    config.titleFontSize);
            readFloat ("subtitleFontSize", config.subtitleFontSize);
            readFloat ("labelFontSize",    config.labelFontSize);
            readFloat ("valueFontSize",    config.valueFontSize);
        }

        return config;
    }

    juce::File themeFile;
    juce::Time lastModTime;
};
// ---- 1. LookAndFeel first, no dependencies ----
class RoomPanLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static constexpr juce::uint32 colourBackground    = 0xffb8c4d0;  // cool blue-grey
    static constexpr juce::uint32 colourPanel         = 0xffc8d4de;  // slightly lighter panel
    static constexpr juce::uint32 colourTrack         = 0xffc2cbd6;  // arc track
    static constexpr juce::uint32 colourTextPrimary   = 0xff2e3238;  // near-black
    static constexpr juce::uint32 colourTextSecondary = 0xff4a5260;  // medium grey-blue
    static constexpr juce::uint32 colourAccent        = 0xffffffff;  // white — the arc
    static constexpr juce::uint32 colourKnob          = 0xff303338;  // dark knob body
    static constexpr juce::uint32 colourKnobDark      = 0xff26282c;  // darker knob body

    RoomPanLookAndFeel();
    void setTheme (const ThemeConfig& newTheme);  // <-- add this line
    ThemeConfig theme; // public so editor can read it in paint() / resized()
    
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                            float sliderPosProportional, float rotaryStartAngle,
                            float rotaryEndAngle, juce::Slider& slider) override;

    void drawLabel (juce::Graphics& g, juce::Label& label) override;
};

// ---- 2. SourcePositionView second ----
// Must be declared before the editor, which holds it as a member.
// Must come after RoomPanLookAndFeel, whose colour tokens it uses in paint().
class SourcePositionView : public juce::Component, private juce::Timer
{
public:
    explicit SourcePositionView (RoomPanAudioProcessor& processorToTrack);
    ~SourcePositionView() override;
    // In SourcePositionView's private section in PluginEditor.h:
    ThemeConfig theme;

    // Public method:
    void setTheme (const ThemeConfig& newTheme);
    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    RoomPanAudioProcessor& processor;

    float displayedAngleRad   = 0.0f;
    float displayedDistance01 = 0.6f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SourcePositionView)
};
class ContentComponent : public juce::Component
{
public:
    ThemeConfig* theme = nullptr;  // pointer set by editor after construction

    void paint (juce::Graphics& g) override
    {
        if (theme == nullptr) return;

        const float w       = (float) getWidth();
        const float headerH = theme->headerHeight;

        // Full background
        g.fillAll (juce::Colour (theme->colourPanel));

        // Header strip
        g.setColour (juce::Colour (theme->colourBackground));
        g.fillRect (0.0f, 0.0f, w, headerH);

        // Title
        g.setFont (juce::Font (juce::FontOptions (theme->titleFontSize)));
        g.setColour (juce::Colour (theme->colourTextPrimary));
        g.drawText ("roompan",
                    juce::Rectangle<float> (16.0f, 10.0f, 200.0f, 28.0f),
                    juce::Justification::centredLeft);

        // Subtitle
        g.setFont (juce::Font (juce::FontOptions (theme->subtitleFontSize)));
        g.setColour (juce::Colour (theme->colourTextSecondary));
        g.drawText ("spatial panner  v0.1",
                    juce::Rectangle<float> (16.0f, 36.0f, 260.0f, 18.0f),
                    juce::Justification::centredLeft);

        // Hairline
        g.setColour (juce::Colour (theme->colourTrack));
        g.drawLine (0.0f, headerH, w, headerH, 1.0f);
    }
};
// ---- 3. Editor last, can now use both types above as members ----
class RoomPanAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    explicit RoomPanAudioProcessorEditor (RoomPanAudioProcessor&);
    ~RoomPanAudioProcessorEditor() override;
    static constexpr float baseWidth  = 500.0f;
    static constexpr float baseHeight = 600.0f;
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    ThemeConfig   theme;         // current live values
    ThemeWatcher  themeWatcher;
    void configureSlider (juce::Slider& slider, juce::Label& label,
                          const juce::String& labelText);
    ContentComponent content;
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
