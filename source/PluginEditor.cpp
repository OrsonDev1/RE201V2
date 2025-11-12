#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
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

    // --- Left column: Head buttons ---
    addAndMakeVisible(head1Button);
    addAndMakeVisible(head2Button);
    addAndMakeVisible(head3Button);

    head1Button.onClick = [this] { processorRef.headEnabled[0] = head1Button.getToggleState(); };
    head2Button.onClick = [this] { processorRef.headEnabled[1] = head2Button.getToggleState(); };
    head3Button.onClick = [this] { processorRef.headEnabled[2] = head3Button.getToggleState(); };

    // --- Right column: Knobs ---
    struct KnobData
    {
        juce::Slider* slider;
        juce::Label* label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>* attachment;
        const char* paramID;
        const char* displayName;
    };

    std::vector<KnobData> knobs =
    {
        { &delayTimeSlider, &delayLabel, &delayTimeAttachment, "delayTime", "Delay Time" },
        { &feedbackSlider, &feedbackLabel, &feedbackAttachment, "feedback", "Feedback" },
        { &saturationSlider, &saturationLabel, &saturationAttachment, "saturation", "Tape Saturation" },
        { &wowSlider, &wowLabel, &wowAttachment, "wow", "Wow" },
        { &flutterSlider, &flutterLabel, &flutterAttachment, "flutter", "Flutter" },
        { &wetDrySlider, &wetDryLabel, &wetDryAttachment, "wetDry", "Wet/Dry" }
    };

    for (auto& k : knobs)
    {
        addAndMakeVisible(*k.slider);
        k.slider->setSliderStyle(juce::Slider::Rotary);
        k.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);

        *k.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
            processorRef.parameters, k.paramID, *k.slider
        );

        k.label->setText(k.displayName, juce::dontSendNotification);
        k.label->setJustificationType(juce::Justification::centred);
        addAndMakeVisible(*k.label);
    }

    setSize(600, 600);
}

PluginEditor::~PluginEditor() {}

void PluginEditor::paint(juce::Graphics& g)
{
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));

    g.setColour(juce::Colours::white);
    g.setFont(16.0f);
    auto title = juce::String("Bonjour from ") + PRODUCT_NAME_WITHOUT_VERSION +
                 " v0.0.5 running in " + CMAKE_BUILD_TYPE;
    g.drawText(title, getLocalBounds().removeFromTop(150), juce::Justification::top, false);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20);

    // --- Inspect button: bottom-left ---
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

    // --- Right: Knobs layout (2 columns, 3 knobs each) ---
    auto rightArea = area;
    int knobSize = 100;
    int labelHeight = 20;
    int verticalSpacing = 40;

    int column1X = rightArea.getX() + rightArea.getWidth() / 4;
    int column2X = rightArea.getX() + rightArea.getWidth() / 2 + 20;

    // First column (Delay, Feedback, Saturation)
    int y = rightArea.getY();
    delayLabel.setBounds(column1X - knobSize / 2, y, knobSize, labelHeight);
    delayTimeSlider.setBounds(column1X - knobSize / 2, y + labelHeight, knobSize, knobSize);
    y += labelHeight + knobSize + verticalSpacing;

    feedbackLabel.setBounds(column1X - knobSize / 2, y, knobSize, labelHeight);
    feedbackSlider.setBounds(column1X - knobSize / 2, y + labelHeight, knobSize, knobSize);
    y += labelHeight + knobSize + verticalSpacing;

    saturationLabel.setBounds(column1X - knobSize / 2, y, knobSize, labelHeight);
    saturationSlider.setBounds(column1X - knobSize / 2, y + labelHeight, knobSize, knobSize);

    // Second column (Wow, Flutter, Wet/Dry)
    y = rightArea.getY();
    wowLabel.setBounds(column2X - knobSize / 2, y, knobSize, labelHeight);
    wowSlider.setBounds(column2X - knobSize / 2, y + labelHeight, knobSize, knobSize);
    y += labelHeight + knobSize + verticalSpacing;

    flutterLabel.setBounds(column2X - knobSize / 2, y, knobSize, labelHeight);
    flutterSlider.setBounds(column2X - knobSize / 2, y + labelHeight, knobSize, knobSize);
    y += labelHeight + knobSize + verticalSpacing;

    wetDryLabel.setBounds(column2X - knobSize / 2, y, knobSize, labelHeight);
    wetDrySlider.setBounds(column2X - knobSize / 2, y + labelHeight, knobSize, knobSize);
}