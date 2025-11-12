#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);

    // --- Inspect button ---
    addAndMakeVisible(inspectButton);
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector>(*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }
        inspector->setVisible(true);
    };

    // --- Delay knob ---
    addAndMakeVisible(delayTimeSlider);
    delayTimeSlider.setSliderStyle(juce::Slider::Rotary);
    delayTimeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    delayTimeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.parameters, "delayTime", delayTimeSlider
    );

    delayLabel.setText("Delay Time", juce::dontSendNotification);
    delayLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(delayLabel);

    // --- Feedback knob ---
    addAndMakeVisible(feedbackSlider);
    feedbackSlider.setSliderStyle(juce::Slider::Rotary);
    feedbackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.parameters, "feedback", feedbackSlider
    );

    feedbackLabel.setText("Feedback", juce::dontSendNotification);
    feedbackLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(feedbackLabel);

    // --- Saturation knob ---
    addAndMakeVisible(saturationSlider);
    saturationSlider.setSliderStyle(juce::Slider::Rotary);
    saturationSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
    saturationAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.parameters, "saturation", saturationSlider
    );

    saturationLabel.setText("Tape Saturation", juce::dontSendNotification);
    saturationLabel.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(saturationLabel);

    // --- Head buttons ---
    addAndMakeVisible(head1Button);
    addAndMakeVisible(head2Button);
    addAndMakeVisible(head3Button);

    head1Button.onClick = [this] { processorRef.headEnabled[0] = head1Button.getToggleState(); };
    head2Button.onClick = [this] { processorRef.headEnabled[1] = head2Button.getToggleState(); };
    head3Button.onClick = [this] { processorRef.headEnabled[2] = head3Button.getToggleState(); };

    // --- Set editor size ---
    setSize(400, 500);
}

PluginEditor::~PluginEditor() {}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    auto area = getLocalBounds();
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    auto helloWorld = juce::String("Bonjour from ") + PRODUCT_NAME_WITHOUT_VERSION +
                      " v0.0.3 running in " + CMAKE_BUILD_TYPE;
    g.drawText(helloWorld, area.removeFromTop(150), juce::Justification::top, false);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20); // overall padding

    // --- Top title area ---
    auto titleArea = area.removeFromTop(30);

    // --- Bottom inspect button ---
    auto bottomArea = area.removeFromBottom(40);
    inspectButton.setBounds(bottomArea.removeFromLeft(120).withHeight(30));

    // --- Left: Head buttons ---
    auto leftArea = area.removeFromLeft(area.getWidth() / 3);
    int buttonHeight = 30;
    int buttonSpacing = 10;

    head1Button.setBounds(leftArea.removeFromTop(buttonHeight).withSizeKeepingCentre(80, buttonHeight));
    leftArea.removeFromTop(buttonSpacing);
    head2Button.setBounds(leftArea.removeFromTop(buttonHeight).withSizeKeepingCentre(80, buttonHeight));
    leftArea.removeFromTop(buttonSpacing);
    head3Button.setBounds(leftArea.removeFromTop(buttonHeight).withSizeKeepingCentre(80, buttonHeight));

    // --- Right: Knobs layout ---
    auto rightArea = area;
    int knobSize = 100;
    int labelHeight = 20;
    int knobSpacing = 30; // vertical spacing between knobs
    int knobCenterX = rightArea.getX() + rightArea.getWidth() / 2;

    // Delay
    delayLabel.setBounds(knobCenterX - knobSize/2, rightArea.getY(), knobSize, labelHeight);
    delayTimeSlider.setBounds(knobCenterX - knobSize/2, rightArea.getY() + labelHeight, knobSize, knobSize);
    rightArea.removeFromTop(labelHeight + knobSize + knobSpacing);

    // Feedback
    feedbackLabel.setBounds(knobCenterX - knobSize/2, rightArea.getY(), knobSize, labelHeight);
    feedbackSlider.setBounds(knobCenterX - knobSize/2, rightArea.getY() + labelHeight, knobSize, knobSize);
    rightArea.removeFromTop(labelHeight + knobSize + knobSpacing);

    // Saturation
    saturationLabel.setBounds(knobCenterX - knobSize/2, rightArea.getY(), knobSize, labelHeight);
    saturationSlider.setBounds(knobCenterX - knobSize/2, rightArea.getY() + labelHeight, knobSize, knobSize);
}