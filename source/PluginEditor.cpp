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

    // FIX: Set initial toggle state to match the processor defaults (true)
    head1Button.setToggleState(processorRef.headEnabled[0], juce::dontSendNotification);
    head2Button.setToggleState(processorRef.headEnabled[1], juce::dontSendNotification);
    head3Button.setToggleState(processorRef.headEnabled[2], juce::dontSendNotification);

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
    setupSlider(reverbMixSlider, reverbMixLabel, "Reverb", "reverbMix");
    setupSlider(masterGainSlider, masterGainLabel, "Master Gain", "masterGain");

    // --- Effect knobs ---
    setupSlider(delayTimeSlider, delayLabel, "Delay Time", "delayTime");
    setupSlider(feedbackSlider, feedbackLabel, "Feedback", "feedback");
    setupSlider(saturationSlider, saturationLabel, "Tape Saturation", "saturation");
    setupSlider(wowSlider, wowLabel, "Wow", "wow");
    setupSlider(flutterSlider, flutterLabel, "Flutter", "flutter");

    setSize(700, 600);

    // --- IR Load Button ---
    addAndMakeVisible(loadIRButton);
    loadIRButton.onClick = [this]
    {
        // 1. Create the File Chooser
        fileChooser = std::make_unique<juce::FileChooser>(
            "Select Impulse Response",
            juce::File::getSpecialLocation(juce::File::userHomeDirectory),
            "*.wav;*.aiff;*.mp3" // Allowed formats
        );

        // 2. Define flags (Open file, ignore directories)
        auto folderChooserFlags = juce::FileBrowserComponent::openMode |
                                  juce::FileBrowserComponent::canSelectFiles;

        // 3. Launch asynchronously (required for modern plugins)
        fileChooser->launchAsync(folderChooserFlags, [this](const juce::FileChooser& fc)
        {
            auto file = fc.getResult();
            if (file.existsAsFile())
            {
                // 4. Pass the file to the processor
                processorRef.loadImpulseResponse(file);
            }
        });
    };
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
                     " v0.0.11 running in " + CMAKE_BUILD_TYPE;
    g.drawText(titleText, getLocalBounds().removeFromTop(150), juce::Justification::top, false);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(20);

    // --- Left Side: Heads & Master Section ---
    int headWidth = 80;
    int headHeight = 40;
    int headSpacing = 20;
    int headStartY = area.getY() + 20;

    // 1. Head Buttons (Top Left)
    head1Button.setBounds(area.getX() + 20, headStartY, headWidth, headHeight);
    head2Button.setBounds(area.getX() + 20, headStartY + headHeight + headSpacing, headWidth, headHeight);
    head3Button.setBounds(area.getX() + 20, headStartY + 2 * (headHeight + headSpacing), headWidth, headHeight);

    // 2. Master Section (Bottom Left)
    int knobSize = 100;
    int labelHeight = 20;
    int knobY = headStartY + 3 * (headHeight + headSpacing) + 30;

    wetDrySlider.setBounds(area.getX() + 20, knobY + labelHeight, knobSize, knobSize);
    wetDryLabel.setBounds(wetDrySlider.getX(), knobY, knobSize, labelHeight);

    masterGainSlider.setBounds(area.getX() + 40 + knobSize, knobY + labelHeight, knobSize, knobSize);
    masterGainLabel.setBounds(masterGainSlider.getX(), knobY, knobSize, labelHeight);

    // --- Right Side: Effects Grid ---
    int rightX = area.getX() + 300;
    int verticalSpacing = 40;
    int colSpacing = 120; // Distance between columns

    // Calculate Row Y positions
    int row1Y = area.getY();
    int row2Y = row1Y + labelHeight + knobSize + verticalSpacing;
    int row3Y = row2Y + labelHeight + knobSize + verticalSpacing;

    // Column 1 (Delay, Feedback, Saturation)
    delayLabel.setBounds(rightX, row1Y, knobSize, labelHeight);
    delayTimeSlider.setBounds(rightX, row1Y + labelHeight, knobSize, knobSize);

    feedbackLabel.setBounds(rightX, row2Y, knobSize, labelHeight);
    feedbackSlider.setBounds(rightX, row2Y + labelHeight, knobSize, knobSize);

    saturationLabel.setBounds(rightX, row3Y, knobSize, labelHeight);
    saturationSlider.setBounds(rightX, row3Y + labelHeight, knobSize, knobSize);

    // Column 2 (Wow, Flutter)
    int col2X = rightX + colSpacing;

    wowLabel.setBounds(col2X, row1Y, knobSize, labelHeight);
    wowSlider.setBounds(col2X, row1Y + labelHeight, knobSize, knobSize);

    flutterLabel.setBounds(col2X, row2Y, knobSize, labelHeight);
    flutterSlider.setBounds(col2X, row2Y + labelHeight, knobSize, knobSize);

    // Column 3 (Reverb Mix) - To the right of Flutter
    int col3X = rightX + (colSpacing * 2);

    reverbMixLabel.setBounds(col3X, row2Y, knobSize, labelHeight);
    reverbMixSlider.setBounds(col3X, row2Y + labelHeight, knobSize, knobSize);

    loadIRButton.setBounds(col3X + 10, row2Y + labelHeight + knobSize + 10, 80, 24);
}