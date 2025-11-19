#include "PluginEditor.h"
#include "PluginProcessor.h"

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
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

    // --- Head buttons ---
    addAndMakeVisible(head1Button);
    addAndMakeVisible(head2Button);
    addAndMakeVisible(head3Button);
    head1Button.onClick = [this] { processorRef.headEnabled[0] = head1Button.getToggleState(); };
    head2Button.onClick = [this] { processorRef.headEnabled[1] = head2Button.getToggleState(); };
    head3Button.onClick = [this] { processorRef.headEnabled[2] = head3Button.getToggleState(); };

    // --- Wet/Dry and Master Gain ---
    auto setupSlider = [&](juce::Slider& slider, juce::Label& label, const char* text, const char* paramID) {
        addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::Rotary);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);

        if (paramID)
            attachments.emplace_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processorRef.parameters, paramID, slider));
    };

    setupSlider(wetDrySlider, wetDryLabel, "Wet/Dry", "wetDry");
    setupSlider(masterGainSlider, masterGainLabel, "Master Gain", "masterGain");

    // --- Effect knobs ---
    setupSlider(delayTimeSlider, delayLabel, "Delay Time", "delayTime");
    setupSlider(feedbackSlider, feedbackLabel, "Feedback", "feedback");
    setupSlider(saturationSlider, saturationLabel, "Tape Saturation", "saturation");
    setupSlider(wowSlider, wowLabel, "Wow", "wow");
    setupSlider(flutterSlider, flutterLabel, "Flutter", "flutter");

    setSize(700, 600);
}

PluginEditor::~PluginEditor() {
    attachments.clear(); // destroy attachments first
}


void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    auto titleText = juce::String("Bonjour from ") + PRODUCT_NAME_WITHOUT_VERSION +
                     " v0.0.9 running in " + CMAKE_BUILD_TYPE;
    g.drawText(titleText, getLocalBounds().removeFromTop(150), juce::Justification::top, false);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(20);

    int headWidth = 80;
    int headHeight = 40;
    int headSpacing = 20;
    int headStartY = area.getY() + 20;

    head1Button.setBounds(area.getX() + 20, headStartY, headWidth, headHeight);
    head2Button.setBounds(area.getX() + 20, headStartY + headHeight + headSpacing, headWidth, headHeight);
    head3Button.setBounds(area.getX() + 20, headStartY + 2 * (headHeight + headSpacing), headWidth, headHeight);

    int knobSize = 100;
    int labelHeight = 20;
    int knobY = headStartY + 3 * (headHeight + headSpacing) + 30;

    wetDrySlider.setBounds(area.getX() + 20, knobY + labelHeight, knobSize, knobSize);
    wetDryLabel.setBounds(wetDrySlider.getX(), knobY, knobSize, labelHeight);

    masterGainSlider.setBounds(area.getX() + 40 + knobSize, knobY + labelHeight, knobSize, knobSize);
    masterGainLabel.setBounds(masterGainSlider.getX(), knobY, knobSize, labelHeight);

    int rightX = area.getX() + 300;
    int verticalSpacing = 40;
    int y = area.getY();

    delayLabel.setBounds(rightX, y, knobSize, labelHeight);
    delayTimeSlider.setBounds(rightX, y + labelHeight, knobSize, knobSize);
    y += labelHeight + knobSize + verticalSpacing;

    feedbackLabel.setBounds(rightX, y, knobSize, labelHeight);
    feedbackSlider.setBounds(rightX, y + labelHeight, knobSize, knobSize);
    y += labelHeight + knobSize + verticalSpacing;

    saturationLabel.setBounds(rightX, y, knobSize, labelHeight);
    saturationSlider.setBounds(rightX, y + labelHeight, knobSize, knobSize);
    y += labelHeight + knobSize + verticalSpacing;

    wowLabel.setBounds(rightX + 120, area.getY(), knobSize, labelHeight);
    wowSlider.setBounds(rightX + 120, area.getY() + labelHeight, knobSize, knobSize);

    flutterLabel.setBounds(rightX + 120, area.getY() + knobSize + labelHeight + verticalSpacing, knobSize, labelHeight);
    flutterSlider.setBounds(rightX + 120, area.getY() + knobSize + labelHeight * 2 + verticalSpacing, knobSize, knobSize);
}