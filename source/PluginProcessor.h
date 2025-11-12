#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- Delay / Feedback ---
    std::vector<float> delayBuffer;
    int writeIndex = 0;

    float feedbackLevel = 0.4f;

    // --- Parameters ---
    juce::AudioProcessorValueTreeState parameters;

    std::atomic<float>* delayTimeParam   = nullptr;
    std::atomic<float>* feedbackParam    = nullptr;
    std::atomic<float>* saturationParam  = nullptr;
    std::atomic<float>* wowParam         = nullptr;
    std::atomic<float>* flutterParam     = nullptr;
    std::atomic<float>* wetDryParam      = nullptr;
    std::atomic<float>* masterGainParam  = nullptr;

    juce::SmoothedValue<float> smoothedDelayTime;

    // --- Delay heads ---
    std::vector<float> headTimesMs = { 150.0f, 300.0f, 450.0f };
    std::vector<float> headLevels  = { 0.6f, 0.4f, 0.3f };
    std::vector<bool>  headEnabled = { true, true, true };

    // --- Wow & flutter ---
    float wowPhase    = 0.0f;
    float flutterPhase= 0.0f;
    float wowRate     = 0.2f;   // Hz
    float flutterRate = 50.0f;  // Hz

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
};