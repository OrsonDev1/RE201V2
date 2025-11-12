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
void PluginProcessor::prepareToPlay (double sampleRate, int /*samplesPerBlock*/)
{
    const float maxDelayTimeMs = 2000.0f * 2.85f; // head3 max
    const size_t maxDelaySamples = static_cast<size_t>(sampleRate * maxDelayTimeMs / 1000.0);
    delayBuffer.resize(maxDelaySamples, 0.0f);
    writeIndex = 0;

    smoothedDelayTime.reset(sampleRate, 0.02); // 20ms smoothing
    smoothedDelayTime.setCurrentAndTargetValue(*delayTimeParam);

    wowPhase = 0.0f;
    flutterPhase = 0.0f;
    wowRate = 0.1f;       // can tweak
    flutterRate = 1.0f;   // can tweak
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

    float delayTimeMs   = *delayTimeParam;
    float feedback      = *feedbackParam;
    float saturation    = *saturationParam;
    float wowAmount     = *wowParam;
    float flutterAmount = *flutterParam;
    float wet           = *wetDryParam;
    float dry           = 1.0f - wet;

    float masterGainDb   = *masterGainParam;
    float masterGainLin  = juce::Decibels::decibelsToGain(masterGainDb);

    const float sampleRate = getSampleRate();
    const size_t delaySamples = static_cast<size_t>(sampleRate * delayTimeMs / 1000.0);

    const size_t bufSize = delayBuffer.size();

    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);

        for (int i = 0; i < numSamples; ++i)
        {
            float inputSample = channelData[i];

            // --- Wow & flutter modulation ---
            float wowMod     = std::sin(wowPhase) * wowAmount * 50.0f;       // max 50 samples
            float flutterMod = std::sin(flutterPhase) * flutterAmount * 5.0f; // max 5 samples

            // small random flutter jitter
            flutterMod += ((rand() / float(RAND_MAX)) - 0.5f) * flutterAmount * 5.0f * 0.3f;

            // modulated read index
            float readIndex = static_cast<float>(writeIndex) - static_cast<float>(delaySamples) + wowMod + flutterMod;
            while (readIndex < 0.0f)
                readIndex += static_cast<float>(bufSize);

            size_t indexA = static_cast<size_t>(static_cast<int>(readIndex)) % bufSize;
            size_t indexB = (indexA + 1) % bufSize;
            float frac = readIndex - static_cast<float>(static_cast<int>(readIndex));

            // linear interpolation
            float delayedSample = delayBuffer[indexA] * (1.0f - frac) + delayBuffer[indexB] * frac;

            // --- Feedback ---
            float processedSample = inputSample + delayedSample * feedback;
            delayBuffer[writeIndex] = processedSample;

            // --- Saturation ---
            processedSample = std::tanh(processedSample * (1.0f + 5.0f * saturation));

            // --- Wet/Dry mix ---
            float outputSample = inputSample * dry + processedSample * wet;

            // --- Apply master gain ---
            outputSample *= masterGainLin;

            // --- Prevent clipping ---
            channelData[i] = juce::jlimit(-1.0f, 1.0f, outputSample);

            // --- Advance write index ---
            writeIndex = (writeIndex + 1) % bufSize;

            // --- Advance LFO phases ---
            wowPhase += 2.0f * juce::MathConstants<float>::pi * wowRate / sampleRate;
            if (wowPhase >= 2.0f * juce::MathConstants<float>::pi)
                wowPhase -= 2.0f * juce::MathConstants<float>::pi;

            flutterPhase += 2.0f * juce::MathConstants<float>::pi * flutterRate / sampleRate;
            if (flutterPhase >= 2.0f * juce::MathConstants<float>::pi)
                flutterPhase -= 2.0f * juce::MathConstants<float>::pi;
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