#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"

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
    std::make_unique<juce::AudioParameterFloat>("feedback", "Feedback", 0.0f, 0.95f, 0.2f),
    std::make_unique<juce::AudioParameterFloat>("saturation", "Saturation", 0.0f, 1.0f, 0.2f),
    std::make_unique<juce::AudioParameterFloat>("wow", "Wow", 0.0f, 1.0f, 0.1f),
    std::make_unique<juce::AudioParameterFloat>("flutter", "Flutter", 0.0f, 1.0f, 0.1f),

    std::make_unique<juce::AudioParameterFloat>("wetDry", "Wet/Dry Mix", 0.0f, 1.0f, 0.5f),

    std::make_unique<juce::AudioParameterFloat>("reverbMix", "Reverb Mix", 0.0f, 1.0f, 0.0f),

    std::make_unique<juce::AudioParameterFloat>("masterGain", "Master Gain", -60.0f, 12.0f, 0.0f)
})

{
    delayTimeParam   = parameters.getRawParameterValue("delayTime");
    feedbackParam    = parameters.getRawParameterValue("feedback");
    saturationParam  = parameters.getRawParameterValue("saturation");
    wowParam         = parameters.getRawParameterValue("wow");
    flutterParam     = parameters.getRawParameterValue("flutter");
    wetDryParam      = parameters.getRawParameterValue("wetDry");

    // --- CONNECT POINTER ---
    reverbMixParam   = parameters.getRawParameterValue("reverbMix");

    masterGainParam  = parameters.getRawParameterValue("masterGain");
}

PluginProcessor::~PluginProcessor()
{
}

//==============================================================================
void PluginProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // --- Delay buffer setup ---
    const float maxDelayTimeMs = 2000.0f * 2.85f;
    const int maxDelaySamples = static_cast<int>(sampleRate * maxDelayTimeMs / 1000.0);

    // Allocate 2 channels (Stereo) with the calculated size
    delayBuffer.setSize(2, maxDelaySamples);
    delayBuffer.clear();

    writeIndex = 0;

    // Smooth delay time changes
    smoothedDelayTime.reset(sampleRate, 0.02); // 20ms smoothing
    if (delayTimeParam)
        smoothedDelayTime.setCurrentAndTargetValue(delayTimeParam->load());

    // --- DSP spec for Convolver and other modules ---
    juce::dsp::ProcessSpec spec {
        sampleRate,
        static_cast<juce::uint32>(samplesPerBlock),
        static_cast<juce::uint32>(getTotalNumOutputChannels())
    };

    reverbConvolver.prepare(spec);

    // --- Modulation LFO init ---
    wowPhase = 0.0f;
    flutterPhase = 0.0f;
    wowRate = 0.1f;
    flutterRate = 1.0f;

    // --- Load embedded impulse response ---
    if (BinaryData::DefaultReverbIR_wavSize > 0)
    {
        reverbConvolver.loadImpulseResponse(
            BinaryData::DefaultReverbIR_wav,
            static_cast<size_t>(BinaryData::DefaultReverbIR_wavSize),
            juce::dsp::Convolution::Stereo::yes,
            juce::dsp::Convolution::Trim::no,
            static_cast<size_t>(BinaryData::DefaultReverbIR_wavSize),
            juce::dsp::Convolution::Normalise::yes
        );
    }

    // Prevent unused warnings
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
    float wet           = 0.5f;
    float reverbMix     = 0.0f;
    float masterGainDb  = 0.0f;

    if (delayTimeParam)   delayTimeMs   = delayTimeParam->load();
    if (feedbackParam)    feedback      = feedbackParam->load();
    if (saturationParam)  saturation    = saturationParam->load();
    if (wowParam)         wowAmount     = wowParam->load();
    if (flutterParam)     flutterAmount = flutterParam->load();
    if (wetDryParam)      wet           = wetDryParam->load();
    if (reverbMixParam)   reverbMix     = reverbMixParam->load();
    if (masterGainParam)  masterGainDb  = masterGainParam->load();

    float dryLevel = 1.0f - wet;

    const float masterGainLin = juce::Decibels::decibelsToGain(masterGainDb);
    const float sampleRate = static_cast<float>(getSampleRate());

    if (delayBuffer.getNumSamples() == 0) return;
    const int bufSize = delayBuffer.getNumSamples();

    // --- ACCURATE RE-201 HEAD TIMINGS ---
    // The heads are not perfectly 1:2:3.
    // Based on RE-201 measurements (ratios approx 1 : 1.9 : 2.75)
    std::vector<float> currentHeadTimesSamples(3);
    currentHeadTimesSamples[0] = (delayTimeMs * 0.364f) * (sampleRate / 1000.0f); // Head 1 (~36%)
    currentHeadTimesSamples[1] = (delayTimeMs * 0.691f) * (sampleRate / 1000.0f); // Head 2 (~69%)
    currentHeadTimesSamples[2] = (delayTimeMs * 1.000f) * (sampleRate / 1000.0f); // Head 3 (100%)

    // === MAIN PROCESSING LOOP ===
    for (int i = 0; i < numSamples; ++i)
    {
        // 1. Update LFOs
        wowPhase += 2.0f * juce::MathConstants<float>::pi * wowRate / sampleRate;
        if (wowPhase >= 2.0f * juce::MathConstants<float>::pi) wowPhase -= 2.0f * juce::MathConstants<float>::pi;

        flutterPhase += 2.0f * juce::MathConstants<float>::pi * flutterRate / sampleRate;
        if (flutterPhase >= 2.0f * juce::MathConstants<float>::pi) flutterPhase -= 2.0f * juce::MathConstants<float>::pi;

        const float wowMod = std::sin(wowPhase) * wowAmount * 50.0f;
        float flutterMod = std::sin(flutterPhase) * flutterAmount * 5.0f;
        flutterMod += ((std::rand() / float(RAND_MAX)) - 0.5f) * flutterAmount * 5.0f * 0.3f;

        // 2. Process Channels
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* channelData = buffer.getWritePointer(ch);
            const int delayCh = ch % delayBuffer.getNumChannels();
            float* delayData = delayBuffer.getWritePointer(delayCh);

            const float inputSample = channelData[i];
            float totalDelaySignal = 0.0f;

            // --- A. Sum Active Playback Heads ---
            for (int head = 0; head < 3; ++head)
            {
                // Access 'headEnabled' from the class member (std::vector<bool>)
                if (!headEnabled[head]) continue;

                // Calculate Read Position
                float readIndex = static_cast<float>(writeIndex) - currentHeadTimesSamples[head] + wowMod + flutterMod;

                while (readIndex < 0.0f) readIndex += static_cast<float>(bufSize);
                while (readIndex >= static_cast<float>(bufSize)) readIndex -= static_cast<float>(bufSize);

                const int readInt = static_cast<int>(readIndex);
                const float frac = readIndex - static_cast<float>(readInt);
                const int indexA = readInt;
                const int indexB = (indexA + 1) % bufSize;

                float sample = delayData[indexA] * (1.0f - frac) + delayData[indexB] * frac;
                totalDelaySignal += sample;
            }

            // --- B. Record Head (Feedback) ---
            float processedSample = inputSample + (totalDelaySignal * feedback);
            processedSample = std::tanh(processedSample * (1.0f + 5.0f * saturation));

            delayData[writeIndex] = processedSample;

            // --- C. Output Mix ---
            channelData[i] = (inputSample * dryLevel) + (totalDelaySignal * wet);
        }

        writeIndex++;
        if (writeIndex >= bufSize) writeIndex = 0;
    }

    // === Convolution Reverb ===
    if (reverbEnabled && reverbMix > 0.0f)
    {
        juce::AudioBuffer<float> wetBuffer;
        wetBuffer.makeCopyOf(buffer);

        juce::dsp::AudioBlock<float> block(wetBuffer);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        reverbConvolver.process(ctx);

        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* mainPtr = buffer.getWritePointer(ch);
            const float* revPtr = wetBuffer.getReadPointer(ch);

            for (int i = 0; i < numSamples; ++i)
                mainPtr[i] = mainPtr[i] * (1.0f - reverbMix) + revPtr[i] * reverbMix;
        }
    }

    // === Master Gain ===
    buffer.applyGain(masterGainLin);

    for (int ch = 0; ch < numChannels; ++ch)
    {
        float* data = buffer.getWritePointer(ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = juce::jlimit(-1.0f, 1.0f, data[i]);
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