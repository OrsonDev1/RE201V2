#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
PluginProcessor::PluginProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
    ),
    parameters(*this, nullptr, "PARAMETERS", {
        std::make_unique<juce::AudioParameterFloat>("delayTime", "Delay Time", 50.0f, 2000.0f, 500.0f),
        std::make_unique<juce::AudioParameterFloat>("feedback", "Feedback", 0.0f, 0.95f, 0.4f),
        std::make_unique<juce::AudioParameterFloat>("saturation", "Saturation", 0.0f, 1.0f, 0.5f),
        std::make_unique<juce::AudioParameterFloat>("wow", "Wow", 0.0f, 1.0f, 0.1f),
        std::make_unique<juce::AudioParameterFloat>("flutter", "Flutter", 0.0f, 1.0f, 0.1f),
        std::make_unique<juce::AudioParameterFloat>("wetDry", "Wet/Dry Mix", 0.0f, 1.0f, 1.0f),
        std::make_unique<juce::AudioParameterFloat>("masterGain", "Master Gain", -60.0f, 12.0f, 0.0f)
    })
{
    delayTimeParam   = parameters.getRawParameterValue("delayTime");
    feedbackParam    = parameters.getRawParameterValue("feedback");
    saturationParam  = parameters.getRawParameterValue("saturation");
    wowParam         = parameters.getRawParameterValue("wow");
    flutterParam     = parameters.getRawParameterValue("flutter");
    wetDryParam      = parameters.getRawParameterValue("wetDry");
    masterGainParam  = parameters.getRawParameterValue("masterGain"); // atomic<float>*
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    const float maxDelayTimeMs = 2000.0f * 2.85f; // head3 max
    const size_t maxDelaySamples = static_cast<size_t>(sampleRate * maxDelayTimeMs / 1000.0);
    delayBuffer.resize(maxDelaySamples, 0.0f);
    writeIndex = 0;

    // Smooth delay time changes
    smoothedDelayTime.reset(sampleRate, 0.02); // 20ms smoothing
    if (delayTimeParam)
        smoothedDelayTime.setCurrentAndTargetValue(delayTimeParam->load());

    // DSP spec used for reverb and other DSP modules
    juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        static_cast<juce::uint32>(getTotalNumOutputChannels())
    };

    reverbConvolver.prepare(spec);

    // Initialize modulation
    wowPhase = 0.0f;
    flutterPhase = 0.0f;
    wowRate = 0.1f;
    flutterRate = 1.0f;

    // Load default impulse response
    auto defaultIR = juce::File::getSpecialLocation(juce::File::currentApplicationFile)
                        .getSiblingFile("Resources/DefaultReverbIR.wav");

    if (defaultIR.existsAsFile())
        reverbConvolver.loadImpulseResponse(defaultIR,
                                            juce::dsp::Convolution::Stereo::yes,
                                            juce::dsp::Convolution::Trim::no,
                                            0);

    // Prevent unused warnings (if ever needed)
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void PluginProcessor::releaseResources()
{
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}

//==============================================================================
void PluginProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int numSamples  = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();

    // --- Parameter reads ---
    float delayTimeMs   = 500.0f;
    float feedback      = 0.4f;
    float saturation    = 0.5f;
    float wowAmount     = 0.0f;
    float flutterAmount = 0.0f;
    float wet           = 1.0f;
    float reverbMix     = 0.0f;
    float masterGainDb  = 0.0f;
    float dryLevel      = 1.0f - wet;

    if (delayTimeParam)   delayTimeMs   = delayTimeParam->load();
    if (feedbackParam)    feedback      = feedbackParam->load();
    if (saturationParam)  saturation    = saturationParam->load();
    if (wowParam)         wowAmount     = wowParam->load();
    if (flutterParam)     flutterAmount = flutterParam->load();
    if (wetDryParam)      wet           = wetDryParam->load();
    if (reverbMixParam)   reverbMix     = reverbMixParam->load();
    if (masterGainParam)  masterGainDb  = masterGainParam->load();
    const float masterGainLin = juce::Decibels::decibelsToGain(masterGainDb);

    const float sampleRate = static_cast<float>(getSampleRate());
    const size_t delaySamples = static_cast<size_t>(sampleRate * delayTimeMs / 1000.0f);
    const size_t bufSize = delayBuffer.size() > 0 ? delayBuffer.size() : 1; // guard

    // === Delay + saturation + wet/dry (fills `buffer`) ===
    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* channelData = buffer.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            const float inputSample = channelData[i];

            // --- Wow & flutter modulation (shared phases) ---
            const float wowMod     = std::sin(wowPhase) * wowAmount * 50.0f;        // max ~50 samples
            float flutterMod       = std::sin(flutterPhase) * flutterAmount * 5.0f;  // max ~5 samples
            flutterMod += ((std::rand() / float(RAND_MAX)) - 0.5f) * flutterAmount * 5.0f * 0.3f;

            // modulated read index (wrap safely)
            float readIndex = static_cast<float>(writeIndex) - static_cast<float>(delaySamples) + wowMod + flutterMod;
            while (readIndex < 0.0f)
                readIndex += static_cast<float>(bufSize);

            const int readInt = static_cast<int>(readIndex);
            const size_t indexA = static_cast<size_t>(readInt) % bufSize;
            const size_t indexB = (indexA + 1) % bufSize;
            const float frac = readIndex - static_cast<float>(readInt);

            // linear interpolation from delayBuffer
            const float delayedSample = delayBuffer[indexA] * (1.0f - frac) + delayBuffer[indexB] * frac;

            // feedback + write into delay buffer
            float processedSample = inputSample + delayedSample * feedback;
            delayBuffer[static_cast<size_t>(writeIndex)] = processedSample;

            // saturation
            processedSample = std::tanh(processedSample * (1.0f + 5.0f * saturation));

            // wet/dry mix (this is the plugin's effect wetness)
            const float dryOut = inputSample * dryLevel;
            const float wetOut = processedSample * wet;
            channelData[i] = dryOut + wetOut;

            // advance write index (shared for all channels)
            writeIndex = (writeIndex + 1) % static_cast<int>(bufSize);

            // advance LFO phases only once per sample (they are global)
            if (ch == numChannels - 1) // only update phases after last channel's sample to avoid doubling
            {
                wowPhase += 2.0f * juce::MathConstants<float>::pi * wowRate / sampleRate;
                if (wowPhase >= 2.0f * juce::MathConstants<float>::pi)
                    wowPhase -= 2.0f * juce::MathConstants<float>::pi;

                flutterPhase += 2.0f * juce::MathConstants<float>::pi * flutterRate / sampleRate;
                if (flutterPhase >= 2.0f * juce::MathConstants<float>::pi)
                    flutterPhase -= 2.0f * juce::MathConstants<float>::pi;
            }
        }
    }

    // === Convolution reverb (process whole buffer once) ===
    if (reverbEnabled) // assume you have a bool flag reverbEnabled in the processor
    {
        // Prepare a temp buffer to receive reverb output
        juce::AudioBuffer<float> wetBuffer;
        wetBuffer.setSize(numChannels, numSamples);
        wetBuffer.clear();

        // Create dsp::AudioBlock wrappers
        juce::dsp::AudioBlock<float> inputBlock (buffer);
        juce::dsp::AudioBlock<float> outputBlock (wetBuffer);

        // Process: input -> output (non-replacing)
        juce::dsp::ProcessContextNonReplacing<float> ctx (inputBlock, outputBlock);
        reverbConvolver.process (ctx);

        // Mix reverb output back into the main buffer using reverbMix
        if (reverbMix > 0.0f)
        {
            for (int ch = 0; ch < numChannels; ++ch)
            {
                float* mainPtr = buffer.getWritePointer(ch);
                float* revPtr  = wetBuffer.getWritePointer(ch);

                for (int i = 0; i < numSamples; ++i)
                    mainPtr[i] = mainPtr[i] * (1.0f - reverbMix) + revPtr[i] * reverbMix;
            }
        }
    }

    // === Master gain (apply after reverb) ===
    if (masterGainLin != 1.0f)
    {
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float s = data[i] * masterGainLin;
                data[i] = juce::jlimit(-1.0f, 1.0f, s);
            }
        }
    }
    else
    {
        // still clip to [-1,1] to be safe
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* data = buffer.getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
                data[i] = juce::jlimit(-1.0f, 1.0f, data[i]);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

bool PluginProcessor::hasEditor() const { return true; }

const juce::String PluginProcessor::getName() const { return JucePlugin_Name; }

bool PluginProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool PluginProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double PluginProcessor::getTailLengthSeconds() const { return 0.0; }
int PluginProcessor::getNumPrograms() { return 1; }
int PluginProcessor::getCurrentProgram() { return 0; }
void PluginProcessor::setCurrentProgram (int) {}
const juce::String PluginProcessor::getProgramName (int) { return {}; }
void PluginProcessor::changeProgramName (int, const juce::String&) {}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData) { juce::ignoreUnused(destData); }
void PluginProcessor::setStateInformation (const void* data, int sizeInBytes) { juce::ignoreUnused(data, sizeInBytes); }

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}