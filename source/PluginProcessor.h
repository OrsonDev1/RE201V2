#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>
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

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    // Core processing
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // UI
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    // Plugin info
    const juce::String getName() const override;
    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    // Programs (not used, but required)
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    // State
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // === Delay system ===
    juce::AudioBuffer<float> delayBuffer;
    int writeIndex = 0;
    float feedbackLevel = 0.4f;

    // Parameters (managed via APVTS)
    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* delayTimeParam   = nullptr;
    std::atomic<float>* feedbackParam    = nullptr;
    std::atomic<float>* saturationParam  = nullptr;
    std::atomic<float>* wowParam         = nullptr;
    std::atomic<float>* flutterParam     = nullptr;
    std::atomic<float>* echoMixParam     = nullptr;
    std::atomic<float>* masterMixParam   = nullptr;
    std::atomic<float>* masterGainParam  = nullptr;
    std::atomic<float>* reverbMixParam   = nullptr;

    juce::SmoothedValue<float> smoothedDelayTime;

    // === Delay heads ===
    std::vector<float> headTimesMs = { 150.0f, 300.0f, 450.0f };
    std::vector<float> headLevels  = { 0.6f, 0.4f, 0.3f };
    std::vector<bool>  headEnabled = { true, true, true };

    // === Wow & flutter ===
    float wowPhase     = 0.0f;
    float flutterPhase = 0.0f;
    float wowRate      = 0.2f;   // Hz
    float flutterRate  = 50.0f;  // Hz

    // === Convolution reverb ===
    juce::dsp::Convolution reverbConvolver;
    juce::File currentIRFile;
    bool useCustomIR = false;
    bool reverbEnabled = true;

    // Helper for IR loading
    void loadImpulseResponse(const juce::File& irFile, bool stereo = true);
    void loadDefaultIR();

    // basic eq params
    std::atomic<float>* bassParam = nullptr;
    std::atomic<float>* trebleParam = nullptr;
    std::vector<juce::IIRFilter> bassFilters;
    std::vector<juce::IIRFilter> trebleFilters;

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
    // We need 2 filters per channel (Bass + Treble).
    // Using a ProcessorChain is the cleanest way in JUCE DSP.
    using FilterType = juce::dsp::IIR::Filter<float>;
    using FilterChain = juce::dsp::ProcessorChain<FilterType, FilterType>; // Filter 1 (Bass), Filter 2 (Treble)
    using StereoFilter = juce::dsp::ProcessorDuplicator<FilterChain, juce::dsp::IIR::Coefficients<float>>;

};