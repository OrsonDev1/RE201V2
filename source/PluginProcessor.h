#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <vector>

class PluginProcessor : public juce::AudioProcessor
{
public:
    PluginProcessor();
    ~PluginProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Delay/feedback stuff
    std::vector<float> delayBuffer;
    int writeIndex = 0;

    float feedbackLevel = 0.4f;

    // APVTS for parameters
    std::atomic<float>* delayTimeParam = nullptr; // pointer to the delay time parameter
    std::atomic<float>* feedbackParam = nullptr; // pointer to feedback time parameter
    juce::AudioProcessorValueTreeState parameters;
    juce::SmoothedValue<float> smoothedDelayTime;

    // Delay head times in ms
    std::vector<float> headTimesMs = { 150.0f, 300.0f, 450.0f };

    // Output level per head
    std::vector<float> headLevels  = { 0.6f, 0.4f, 0.3f };

    // On/off state for each head
    std::vector<bool> headEnabled  = { true, true, true };


    //Saturation paras
    std::atomic<float>* saturationParam = nullptr;

    //wow and flutter
    std::atomic<float>* wowParam = nullptr;
    std::atomic<float>* flutterParam = nullptr;
    std::atomic<float>* wetDryParam = nullptr;

    float wowPhase = 0.0f;
    float flutterPhase = 0.0f;
    float wowRate = 0.2f;    // Hz, slow wobble
    float flutterRate = 50.0f; // Hz, faster jitter




private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginProcessor)
};