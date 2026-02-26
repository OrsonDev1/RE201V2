#pragma once



#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include <vector>

class OverloadLED : public juce::Component
{
public:
    void setBrightness(float b) {
        brightness = juce::jlimit(0.0f, 1.0f, b);
        repaint();
    }
    void paint(juce::Graphics& g) override {
        auto bounds = getLocalBounds().toFloat().reduced(2.0f);
        // Dark red background (LED Off)
        g.setColour(juce::Colour(0xff400000));
        g.fillEllipse(bounds);

        // Bright red + glow (LED On)
        if (brightness > 0.0f) {
            g.setColour(juce::Colours::red.withAlpha(brightness));
            g.fillEllipse(bounds);
            // White hot center
            g.setColour(juce::Colours::white.withAlpha(brightness * 0.6f));
            g.fillEllipse(bounds.reduced(bounds.getWidth() * 0.3f));
        }
        // Bezel outline
        g.setColour(juce::Colours::black);
        g.drawEllipse(bounds, 1.5f);
    }
private:
    float brightness = 0.0f;
};

class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RetroLookAndFeel()
    {
        // Set standard text colours
        setColour(juce::Slider::textBoxTextColourId, juce::Colours::black);
        setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    }

    // --- 1. VINTAGE KNOBS (Silver & Dark Grey) ---
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
        const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {
        auto radius = (float)juce::jmin(width / 2, height / 2) - 4.0f;
        auto centreX = (float)x + (float)width * 0.5f;
        auto centreY = (float)y + (float)height * 0.5f;
        auto rx = centreX - radius;
        auto ry = centreY - radius;
        auto rw = radius * 2.0f;
        auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

        // A. Knob Body (Dark recessed circle)
        g.setColour(juce::Colour(0xff202020));
        g.fillEllipse(rx, ry, rw, rw);

        // B. Knob Outline (Silver ring)
        g.setColour(juce::Colours::grey);
        g.drawEllipse(rx, ry, rw, rw, 2.0f);

        // C. Pointer (White Line)
        juce::Path p;
        auto pointerLength = radius * 0.8f;
        auto pointerThickness = 3.0f;
        p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
        p.applyTransform(juce::AffineTransform::rotation(angle).translated(centreX, centreY));

        g.setColour(juce::Colours::white);
        g.fillPath(p);
    }

    // --- 2. TAPE SWITCHES (Green LED Style) ---
    void drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
        bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto area = button.getLocalBounds().reduced(2);

        // A. Background (Dark Plastic)
        g.setColour(juce::Colour(0xccff0000));
        g.fillRoundedRectangle(area.toFloat(), 4.0f);
        g.setColour(juce::Colours::grey);
        g.drawRoundedRectangle(area.toFloat(), 4.0f, 8.0f);

        // B. Active State (Green LED light)
        if (button.getToggleState())
        {
            // Main Light
            g.setColour(juce::Colours::lightgreen.withAlpha(0.9f));
            g.fillRoundedRectangle(area.reduced(4).toFloat(), 3.0f);

            // Outer Glow
            g.setColour(juce::Colours::lightgreen.withAlpha(0.4f));
            g.fillRoundedRectangle(area.reduced(2).toFloat(), 4.0f);
        }

        // C. Text Label
        g.setColour(juce::Colours::black);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(button.getButtonText(), area, juce::Justification::centred, false);
    }

    // Optional: Ensure labels are bold
    juce::Font getLabelFont(juce::Label&) override
    {
        return juce::Font(14.0f, juce::Font::bold);
    }
};

//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    explicit PluginEditor (PluginProcessor&);
    ~PluginEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> attachments;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    PluginProcessor& processorRef;
    //melatonin - remove before final
    std::unique_ptr<melatonin::Inspector> inspector;
    juce::TextButton inspectButton { "Inspect the UI" };
    //delay + feedback slider
    juce::Slider delayTimeSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> delayTimeAttachment;
    juce::Slider feedbackSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> feedbackAttachment;
    //saturation
    juce::Slider saturationSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> saturationAttachment;
    juce::Slider wowSlider;
    juce::Slider flutterSlider;
    //wet dry
    juce::Slider masterMixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterMixAttachment;
    juce::Slider masterGainSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> masterGainAttachment;
    juce::Slider reverbMixSlider;
    juce::Slider echoMixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> echoMixAttachment;\
    juce::Slider bassSlider;
    juce::Slider trebleSlider;
    juce::TextButton resetIRButton { "X" }; // Small reset button

    RetroLookAndFeel myLookAndFeel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> head1Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> head2Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> head3Attachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> wowAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> flutterAttachment;

    juce::ToggleButton head1Button { "Head 1" };
    juce::ToggleButton head2Button { "Head 2" };
    juce::ToggleButton head3Button { "Head 3" };
    juce::Label delayLabel;
    juce::Label feedbackLabel;
    juce::Label saturationLabel;
    juce::Label wowLabel;
    juce::Label flutterLabel;
    juce::Label masterMixLabel;
    juce::Label masterGainLabel;
    juce::Label reverbMixLabel;
    juce::Label echoMixLabel;
    juce::Label bassLabel;
    juce::Label trebleLabel;
    juce::TextButton loadIRButton { "Load IR" };
    std::unique_ptr<juce::FileChooser> fileChooser;
    juce::ToggleButton bypassButton { "Bypass" };
    juce::ToggleButton killDryButton { "Kill Dry" };
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> bypassAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> killDryAttachment;
    juce::TextButton initButton { "Reset All" };

    juce::Slider inputGainSlider;
    juce::Label inputGainLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> inputGainAttachment;

    juce::SharedResourcePointer<juce::TooltipWindow> tooltipWindow;

    OverloadLED peakLed;
    float ledDecay = 0.0f;

    void timerCallback() override;


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
