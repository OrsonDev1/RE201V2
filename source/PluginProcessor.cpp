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

    std::make_unique<juce::AudioParameterFloat>("wetDry", "Master Mix", 0.0f, 1.0f, 0.5f),

    std::make_unique<juce::AudioParameterFloat>("reverbMix", "Reverb Mix", 0.0f, 1.0f, 0.0f),
    std::make_unique<juce::AudioParameterFloat>("echoMix", "Echo Mix", 0.0f, 1.0f, 0.5f),
    std::make_unique<juce::AudioParameterFloat>("masterGain", "Master Gain", -60.0f, 12.0f, 0.0f)

})

{
    delayTimeParam   = parameters.getRawParameterValue("delayTime");
    feedbackParam    = parameters.getRawParameterValue("feedback");
    saturationParam  = parameters.getRawParameterValue("saturation");
    wowParam         = parameters.getRawParameterValue("wow");
    flutterParam     = parameters.getRawParameterValue("flutter");
    masterMixParam      = parameters.getRawParameterValue("wetDry");
    echoMixParam        = parameters.getRawParameterValue ("echoMix");
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

    // --- 1. Load Parameters ---
    float delayTimeMs   = 500.0f;
    float feedback      = 0.4f;
    float saturation    = 0.5f;
    float wowAmount     = 0.0f;
    float flutterAmount = 0.0f;
    float echoVol       = 0.5f;  // Controls Tape Volume
    float reverbVol     = 0.0f;  // Controls Reverb Volume
    float masterMix     = 0.5f;  // Controls Global Dry/Wet
    float masterGainDb  = 0.0f;

    if (delayTimeParam)   delayTimeMs   = delayTimeParam->load();
    if (feedbackParam)    feedback      = feedbackParam->load();
    if (saturationParam)  saturation    = saturationParam->load();
    if (wowParam)         wowAmount     = wowParam->load();
    if (flutterParam)     flutterAmount = flutterParam->load();

    if (echoMixParam)     echoVol       = echoMixParam->load();
    if (reverbMixParam)   reverbVol     = reverbMixParam->load();
    if (masterMixParam)   masterMix     = masterMixParam->load(); // Linked to "wetDry" ID
    if (masterGainParam)  masterGainDb  = masterGainParam->load();

    // Prepare Master Mix Gains (Constant Power Law)
    // This prevents volume drop in the middle of the mix knob
    float globalDryGain = std::cos(masterMix * juce::MathConstants<float>::halfPi);
    float globalWetGain = std::sin(masterMix * juce::MathConstants<float>::halfPi);

    // --- 2. Prepare Buffers ---
    // Snapshot the Clean Dry Input
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // Create a "Wet Layer" buffer to accumulate Echo + Reverb
    juce::AudioBuffer<float> wetAccumulator;
    wetAccumulator.setSize(numChannels, numSamples);
    wetAccumulator.clear(); // Start silent

    if (delayBuffer.getNumSamples() == 0) return;
    const int bufSize = delayBuffer.getNumSamples();
    const float sampleRate = static_cast<float>(getSampleRate());

    // --- 3. Calculate Head Timings ---
    std::vector<float> currentHeadTimesSamples(3);
    currentHeadTimesSamples[0] = (delayTimeMs * 0.364f) * (sampleRate / 1000.0f); // Head 1
    currentHeadTimesSamples[1] = (delayTimeMs * 0.691f) * (sampleRate / 1000.0f); // Head 2
    currentHeadTimesSamples[2] = (delayTimeMs * 1.000f) * (sampleRate / 1000.0f); // Head 3

    // === 4. TAPE ECHO PROCESSING LOOP ===
    for (int i = 0; i < numSamples; ++i)
    {
        // A. Update LFOs
        wowPhase += 2.0f * juce::MathConstants<float>::pi * wowRate / sampleRate;
        if (wowPhase >= 2.0f * juce::MathConstants<float>::pi) wowPhase -= 2.0f * juce::MathConstants<float>::pi;

        flutterPhase += 2.0f * juce::MathConstants<float>::pi * flutterRate / sampleRate;
        if (flutterPhase >= 2.0f * juce::MathConstants<float>::pi) flutterPhase -= 2.0f * juce::MathConstants<float>::pi;

        const float wowMod = std::sin(wowPhase) * wowAmount * 50.0f;
        float flutterMod = std::sin(flutterPhase) * flutterAmount * 5.0f;
        flutterMod += ((std::rand() / float(RAND_MAX)) - 0.5f) * flutterAmount * 5.0f * 0.3f;

        // B. Process Channels
        for (int ch = 0; ch < numChannels; ++ch)
        {
            float* delayData = delayBuffer.getWritePointer(ch % 2);
            float inputSample = dryBuffer.getReadPointer(ch)[i]; // Read from DRY snapshot

            // --- Sum Active Heads ---
            float rawEchoSample = 0.0f;
            for (int head = 0; head < 3; ++head)
            {
                if (!headEnabled[head]) continue;

                float readIndex = static_cast<float>(writeIndex) - currentHeadTimesSamples[head] + wowMod + flutterMod;

                while (readIndex < 0.0f) readIndex += static_cast<float>(bufSize);
                while (readIndex >= static_cast<float>(bufSize)) readIndex -= static_cast<float>(bufSize);

                const int readInt = static_cast<int>(readIndex);
                const float frac = readIndex - static_cast<float>(readInt);
                const int indexA = readInt;
                const int indexB = (indexA + 1) % bufSize;

                float sample = delayData[indexA] * (1.0f - frac) + delayData[indexB] * frac;

                // IMPORTANT: Apply head volume levels here
                rawEchoSample += sample * headLevels[head];
            }

            // --- Feedback Loop ---
            float feedbackSample = inputSample + (rawEchoSample * feedback);
            feedbackSample = std::tanh(feedbackSample * (1.0f + 5.0f * saturation));
            delayData[writeIndex] = feedbackSample;

            // --- Add to Wet Accumulator ---
            // We multiply by Echo Volume here
            wetAccumulator.getWritePointer(ch)[i] += rawEchoSample * echoVol;
        }

        writeIndex++;
        if (writeIndex >= bufSize) writeIndex = 0;
    }

    // === 5. REVERB PROCESSING ===
    if (reverbEnabled && reverbVol > 0.0f)
    {
        // We use the Dry Buffer as the source for the Reverb (Parallel)
        juce::AudioBuffer<float> reverbInput;
        reverbInput.makeCopyOf(dryBuffer);

        juce::dsp::AudioBlock<float> block(reverbInput);
        juce::dsp::ProcessContextReplacing<float> ctx(block);
        reverbConvolver.process(ctx);

        // Mix Reverb into the Wet Accumulator
        for (int ch = 0; ch < numChannels; ++ch)
        {
            wetAccumulator.addFrom(ch, 0, reverbInput, ch, 0, numSamples, reverbVol);
        }
    }

    // === 6. FINAL MIX & OUTPUT ===
    // Blend Dry Buffer and Wet Accumulator into the Main Output Buffer
    for (int ch = 0; ch < numChannels; ++ch)
    {
        auto* out = buffer.getWritePointer(ch);
        const auto* dry = dryBuffer.getReadPointer(ch);
        const auto* wet = wetAccumulator.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            // Global Mix Blend
            out[i] = (dry[i] * globalDryGain) + (wet[i] * globalWetGain);

            // Safety Limiter
            out[i] = juce::jlimit(-1.0f, 1.0f, out[i]);
        }
    }

    // Apply Master Gain
    buffer.applyGain(juce::Decibels::decibelsToGain(masterGainDb));
}

void PluginProcessor::loadImpulseResponse(const juce::File& irFile, bool stereo)
{
    // Check if file actually exists before trying to load
    if (!irFile.existsAsFile()) return;

    // Load the file into the convolution engine
    // We use Trim::yes to remove silence at the start/end
    // We use Normalise::yes to ensure it doesn't blow up the volume
    reverbConvolver.loadImpulseResponse(
        irFile,
        stereo ? juce::dsp::Convolution::Stereo::yes : juce::dsp::Convolution::Stereo::no,
        juce::dsp::Convolution::Trim::yes,
        0,
        juce::dsp::Convolution::Normalise::yes
    );
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