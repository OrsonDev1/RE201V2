#pragma once



#include "PluginProcessor.h"
#include "BinaryData.h"
#include "melatonin_inspector/melatonin_inspector.h"
#include <vector>


//==============================================================================
class PluginEditor : public juce::AudioProcessorEditor
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


    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginEditor)
};
