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
        std::make_unique<juce::AudioParameterFloat>("flutter", "Flutter", 0.0f, 1.0f, 0.1f)
    })
{
    // ← Initialize the pointer here
    delayTimeParam = parameters.getRawParameterValue("delayTime");
    feedbackParam = parameters.getRawParameterValue("feedback");
    saturationParam = parameters.getRawParameterValue("saturation");
    wowParam = parameters.getRawParameterValue("wow");
    flutterParam = parameters.getRawParameterValue("flutter");
}

PluginProcessor::~PluginProcessor()
{

}

//==============================================================================
void PluginProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    const float maxDelayTimeMs = 2000.0f * 2.85f; // head3 max
    const int maxDelaySamples = static_cast<int>(sampleRate * maxDelayTimeMs / 1000.0);
    delayBuffer.resize(maxDelaySamples, 0.0f);
    writeIndex = 0;
    smoothedDelayTime.reset(sampleRate, 0.02); // 20ms smoothing
    smoothedDelayTime.setCurrentAndTargetValue(*delayTimeParam);
}

void PluginProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
        && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
    #if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
    #endif

    return true;
#endif
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    const int numSamples = buffer.getNumSamples();
    const int numChannels = buffer.getNumChannels();
    float delayTimeMs = *delayTimeParam;
    float feedback = *feedbackParam;
    float saturation = *saturationParam;
    float wowAmount = *wowParam;
    float flutterAmount = *flutterParam;

    // convert delayTimeMs to samples
    int delaySamples = static_cast<int>(getSampleRate() * delayTimeMs / 1000.0f);

    for (int channel = 0; channel < numChannels; ++channel)
    {
        float* channelData = buffer.getWritePointer(channel);

        for (int i = 0; i < numSamples; ++i)
        {
            float inputSample = channelData[i];

            // --- Calculate wow & flutter modulation ---
            float wowMod = std::sin(wowPhase) * wowAmount * 50.0f;       // max 50 samples
            float flutterMod = std::sin(flutterPhase) * flutterAmount * 5.0f; // max 5 samples

            // optional: small random flutter jitter
            flutterMod += ((rand() / float(RAND_MAX)) - 0.5f) * flutterAmount * 5.0f * 0.3f;

            // final read index with modulation
            float readIndexMod = writeIndex - delaySamples + wowMod + flutterMod;
            if (readIndexMod < 0.0f)
                readIndexMod += delayBuffer.size();

            int indexA = static_cast<int>(readIndexMod) % delayBuffer.size();
            int indexB = (indexA + 1) % delayBuffer.size();
            float frac = readIndexMod - static_cast<float>(indexA);

            float delayedSample = delayBuffer[indexA] * (1.0f - frac) + delayBuffer[indexB] * frac;

            // --- Apply feedback and store in buffer ---
            float outputSample = inputSample + delayedSample * feedback;
            delayBuffer[writeIndex] = outputSample;

            // --- Apply tape saturation ---
            outputSample = std::tanh(outputSample * (1.0f + 5.0f * saturation));

            // write final sample
            channelData[i] = outputSample;

            // advance write index
            writeIndex = (writeIndex + 1) % delayBuffer.size();

            // advance LFO phases per sample
            wowPhase += 2.0f * juce::MathConstants<float>::pi * wowRate / getSampleRate();
            if (wowPhase >= 2.0f * juce::MathConstants<float>::pi) wowPhase -= 2.0f * juce::MathConstants<float>::pi;

            flutterPhase += 2.0f * juce::MathConstants<float>::pi * flutterRate / getSampleRate();
            if (flutterPhase >= 2.0f * juce::MathConstants<float>::pi) flutterPhase -= 2.0f * juce::MathConstants<float>::pi;
        }
    }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

//==============================================================================
bool PluginProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

//==============================================================================
const juce::String PluginProcessor::getName() const
{
    return JucePlugin_Name;
}

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

double PluginProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int PluginProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
        // so this should be at least 1, even if you're not really implementing programs.
}

int PluginProcessor::getCurrentProgram()
{
    return 0;
}

void PluginProcessor::setCurrentProgram (int index)
{
    juce::ignoreUnused (index);
}

const juce::String PluginProcessor::getProgramName (int index)
{
    juce::ignoreUnused (index);
    return {};
}

void PluginProcessor::changeProgramName (int index, const juce::String& newName)
{
    juce::ignoreUnused (index, newName);
}

//==============================================================================
void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::ignoreUnused (destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ignoreUnused (data, sizeInBytes);
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
