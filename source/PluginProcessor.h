#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <vector>


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
    std::atomic<float>* head1Param = nullptr;
    std::atomic<float>* head2Param = nullptr;
    std::atomic<float>* head3Param = nullptr;
    std::atomic<float>* bypassParam = nullptr;
    std::atomic<float>* killDryParam = nullptr;

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

    std::atomic<float>* inputGainParam = nullptr;
    std::atomic<float> inputPeakLevel { 0.0f };

private:
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(PluginProcessor)
    // We need 2 filters per channel (Bass + Treble).
    // Using a ProcessorChain is the cleanest way in JUCE DSP.
    using FilterType = juce::dsp::IIR::Filter<float>;
    using FilterChain = juce::dsp::ProcessorChain<FilterType, FilterType>; // Filter 1 (Bass), Filter 2 (Treble)
    using StereoFilter = juce::dsp::ProcessorDuplicator<FilterChain, juce::dsp::IIR::Coefficients<float>>;

};