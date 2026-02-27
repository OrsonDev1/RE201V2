#include "PluginEditor.h"
#include "PluginProcessor.h"

PluginEditor::PluginEditor(PluginProcessor& p)
    : AudioProcessorEditor(&p), processorRef(p)
{
    setLookAndFeel(&myLookAndFeel);
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

    head1Attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.parameters, "head1", head1Button);
    head2Attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.parameters, "head2", head2Button);
    head3Attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.parameters, "head3", head3Button);

    head1Button.setColour(juce::ToggleButton::tickColourId, juce::Colours::black);
    head2Button.setColour(juce::ToggleButton::tickColourId, juce::Colours::black);
    head3Button.setColour(juce::ToggleButton::tickColourId, juce::Colours::black);

    //bypass and dry kill buttons
    addAndMakeVisible(bypassButton);
    addAndMakeVisible(killDryButton);

    bypassButton.setColour(juce::ToggleButton::textColourId, juce::Colours::black);
    killDryButton.setColour(juce::ToggleButton::textColourId, juce::Colours::black);

    bypassAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.parameters, "bypass", bypassButton);
    killDryAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.parameters, "killDry", killDryButton);

    addAndMakeVisible(syncButton);
    syncButton.setColour(juce::ToggleButton::textColourId, juce::Colours::black);
    syncButton.setTooltip("Locks the delay time to your DAW's tempo.");
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(processorRef.parameters, "syncMode", syncButton);

    addAndMakeVisible(syncRateBox);
    syncRateBox.addItemList({"1/2", "1/4", "1/4 Dotted", "1/4 Triplet", "1/8", "1/8 Dotted", "1/8 Triplet", "1/16"}, 1);
    syncRateBox.setJustificationType(juce::Justification::centred);
    syncRateAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(processorRef.parameters, "syncRate", syncRateBox);

    // Polish: Grey out the free-time knob when Sync is enabled
    syncButton.onClick = [this] { delayTimeSlider.setEnabled(!syncButton.getToggleState()); };
    delayTimeSlider.setEnabled(!syncButton.getToggleState()); // Set initial state

    //reset
    addAndMakeVisible(initButton);
    initButton.setTooltip("Resets all parameters to their default 'Ground Zero' values.");

    // The Reset Logic
    initButton.onClick = [this]
    {
        // Loop through every single parameter in the plugin
        for (auto* param : processorRef.getParameters())
        {
            if (auto* p = dynamic_cast<juce::AudioProcessorParameterWithID*>(param))
            {
                // Snap it back to its default value, and notify the DAW so the UI updates
                p->beginChangeGesture();
                p->setValueNotifyingHost(p->getDefaultValue());
                p->endChangeGesture();
            }
        }
    };

    // --- Wet/Dry and Master Gain ---
    auto setupSlider = [&](juce::Slider& slider, juce::Label& label, const char* text, const char* paramID) {
        addAndMakeVisible(slider);
        slider.setSliderStyle(juce::Slider::Rotary);
        slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);

        label.setText(text, juce::dontSendNotification);
        label.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(label);
        label.setColour(juce::Label::textColourId, juce::Colours::black);

        if (paramID)
            attachments.emplace_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
                processorRef.parameters, paramID, slider));
    };

    setupSlider(masterMixSlider, masterMixLabel, "Master Mix", "wetDry");
    setupSlider(reverbMixSlider, reverbMixLabel, "Reverb Mix", "reverbMix");
    setupSlider(masterGainSlider, masterGainLabel, "Master Gain", "masterGain");
    setupSlider(echoMixSlider, echoMixLabel, "Echo Mix", "echoMix");

    setupSlider(inputGainSlider, inputGainLabel, "Input Gain", "inputGain");
    addAndMakeVisible(peakLed);
    startTimerHz(30); // Start the LED update timer

    // --- Effect knobs ---
    setupSlider(delayTimeSlider, delayLabel, "Delay Time", "delayTime");
    setupSlider(feedbackSlider, feedbackLabel, "Feedback", "feedback");
    setupSlider(saturationSlider, saturationLabel, "Tape Saturation", "saturation");
    setupSlider(wowSlider, wowLabel, "Wow", "wow");
    setupSlider(flutterSlider, flutterLabel, "Flutter", "flutter");
    setupSlider(bassSlider, bassLabel, "Bass", "bass");
    setupSlider(trebleSlider, trebleLabel, "Treble", "treble");

    // tooltips
    delayTimeSlider.setTooltip("Adjusts the tape read head distance (50ms - 600ms).");
    feedbackSlider.setTooltip("Feeds the echoes back into the tape. Warning: High values will self-oscillate!");
    saturationSlider.setTooltip("Drives the signal into the magnetic tape for harmonic distortion.");
    wowSlider.setTooltip("Simulates slow tape motor inconsistencies.");
    flutterSlider.setTooltip("Simulates fast tape crinkle and mechanical wear.");
    killDryButton.setTooltip("Mutes the dry signal.");
    bassSlider.setTooltip ("This sets the Low shelf that effects the Wet Signal. It is at 150Hz");
    trebleSlider.setTooltip ("This sets the High Shelf that effects the Wet Signal. It is set at 3KHz");
    inputGainSlider.setTooltip ("This changes the level of signal that is coming into the plugin. An overload indicator is provided just above the knob.");
    resetIRButton.setTooltip ("This resets the IR back to stock if a custom one is set.");



    setSize(850, 480);

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
    // --- Reset IR Button ---
    addAndMakeVisible(resetIRButton);
    resetIRButton.setTooltip("Reset to Stock Reverb");
    resetIRButton.onClick = [this] { processorRef.loadDefaultIR(); };
}

PluginEditor::~PluginEditor() {
    setLookAndFeel(nullptr);
    attachments.clear(); // destroy attachments first
}


void PluginEditor::paint(juce::Graphics& g)
{
    // 1. Background (The Case - light grey Tolex style)
    g.fillAll(juce::Colour(0x6dc1cbc1));

    // 2. The Faceplate (Green/Black area where knobs live)
    auto area = getLocalBounds().reduced(10);
    auto faceplate = area.removeFromBottom(area.getHeight() - 50); // Leave room for top title

    g.setColour(juce::Colour(0xcc24a12a)); // green panel
    g.fillRoundedRectangle(faceplate.toFloat(), 10.0f);
    g.setColour(juce::Colours::black);
    g.drawRoundedRectangle(faceplate.toFloat(), 10.0f, 2.0f);

    // 3. Draw Section Dividers (Lines)
    g.setColour(juce::Colours::black.withAlpha(0.3f));

    // The line now draws all the way to the bottom (faceplate.getBottom() - 10.0f)
    g.drawLine(170.0f, faceplate.getY() + 10.0f, 170.0f, faceplate.getBottom() - 10.0f, 1.0f);

    // 4. Title
    g.setColour(juce::Colours::black);
    g.setFont(24.0f);
    g.drawText("Cosmic Tape Delay 201 Version 0.9.2 Beta", area.removeFromTop(40), juce::Justification::centred, false);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(40); // Skip title

    // --- 1. LEFT COLUMN (Now spans the entire height of the UI) ---
    auto leftCol = area.removeFromLeft(150);
    int buttonHeight = 35;
    int startY = leftCol.getY() + 15;

    // Heads
    head1Button.setBounds(leftCol.getX() + 10, startY, 100, buttonHeight);
    head2Button.setBounds(leftCol.getX() + 10, startY + 45, 100, buttonHeight);
    head3Button.setBounds(leftCol.getX() + 10, startY + 90, 100, buttonHeight);

    // Tempo Sync
    syncButton.setBounds(leftCol.getX() + 10, startY + 140, 100, buttonHeight);
    syncRateBox.setBounds(leftCol.getX() + 10, startY + 180, 100, 25);

    // Utilities
    bypassButton.setBounds(leftCol.getX() + 10, startY + 230, 100, buttonHeight);
    killDryButton.setBounds(leftCol.getX() + 10, startY + 275, 100, buttonHeight);
    initButton.setBounds(leftCol.getX() + 10, startY + 320, 100, buttonHeight);

    // Add a visual gap between the vertical line and the right-side controls
    area.removeFromLeft(20);

    // --- 2. BOTTOM MIXER STRIP (Now only spans the right side) ---
    auto bottomStrip = area.removeFromBottom(120);
    int mixKnobWidth = bottomStrip.getWidth() / 5;

    // 1. INPUT GAIN & LED
    int col0 = bottomStrip.getX();
    inputGainLabel.setBounds(col0, bottomStrip.getY(), mixKnobWidth, 20);
    inputGainSlider.setBounds(col0, bottomStrip.getY() + 20, mixKnobWidth, 80);
    peakLed.setBounds(col0 + mixKnobWidth - 30, bottomStrip.getY() + 10, 15, 15);

    // 2. ECHO MIX
    int col1 = bottomStrip.getX() + mixKnobWidth;
    echoMixLabel.setBounds(col1, bottomStrip.getY(), mixKnobWidth, 20);
    echoMixSlider.setBounds(col1, bottomStrip.getY() + 20, mixKnobWidth, 80);

    // 3. REVERB MIX
    int col2 = bottomStrip.getX() + mixKnobWidth * 2;
    reverbMixLabel.setBounds(col2, bottomStrip.getY(), mixKnobWidth, 20);
    reverbMixSlider.setBounds(col2, bottomStrip.getY() + 20, mixKnobWidth, 80);

    int buttonY = reverbMixSlider.getBottom() + 5;
    int loadBtnWidth = mixKnobWidth - 40;
    loadIRButton.setBounds(col2 + 5, buttonY, loadBtnWidth, 20);
    resetIRButton.setBounds(loadIRButton.getRight() + 5, buttonY, 25, 20);

    // 4. MASTER MIX
    int col3 = bottomStrip.getX() + mixKnobWidth * 3;
    masterMixLabel.setBounds(col3, bottomStrip.getY(), mixKnobWidth, 20);
    masterMixSlider.setBounds(col3, bottomStrip.getY() + 20, mixKnobWidth, 80);

    // 5. MASTER GAIN
    int col4 = bottomStrip.getX() + mixKnobWidth * 4;
    masterGainLabel.setBounds(col4, bottomStrip.getY(), mixKnobWidth, 20);
    masterGainSlider.setBounds(col4, bottomStrip.getY() + 20, mixKnobWidth, 80);

    // --- 3. MAIN EFFECTS GRID (Repacked for balance) ---
    int knobSize = 90;
    int spacing = 35; // Wider spacing to fill the new width
    int vSpacing = 15;

    int gridStartX = area.getX() + 10;
    int gridStartY = area.getY() + 10;

    // Pre-calculate column X positions
    int x1 = gridStartX;
    int x2 = x1 + knobSize + spacing;
    int x3 = x2 + knobSize + spacing;
    int x4 = x3 + knobSize + spacing;

    // --- ROW 1: Delay | Feedback | Saturation ---
    int row1Y = gridStartY;

    delayLabel.setBounds(x1, row1Y, knobSize, 20);
    delayTimeSlider.setBounds(x1, row1Y + 20, knobSize, knobSize);

    feedbackLabel.setBounds(x2, row1Y, knobSize, 20);
    feedbackSlider.setBounds(x2, row1Y + 20, knobSize, knobSize);

    saturationLabel.setBounds(x3 - 10, row1Y, knobSize + 20, 20);
    saturationSlider.setBounds(x3, row1Y + 20, knobSize, knobSize);

    // --- ROW 2: Wow | Flutter | Bass | Treble ---
    int row2Y = row1Y + knobSize + 20 + vSpacing;

    wowLabel.setBounds(x1, row2Y, knobSize, 20);
    wowSlider.setBounds(x1, row2Y + 20, knobSize, knobSize);

    flutterLabel.setBounds(x2, row2Y, knobSize, 20);
    flutterSlider.setBounds(x2, row2Y + 20, knobSize, knobSize);

    bassLabel.setBounds(x3, row2Y, knobSize, 20);
    bassSlider.setBounds(x3, row2Y + 20, knobSize, knobSize);

    trebleLabel.setBounds(x4, row2Y, knobSize, 20);
    trebleSlider.setBounds(x4, row2Y + 20, knobSize, knobSize);
}

void PluginEditor::timerCallback()
{
    // 0.95f is almost clipping (digital absolute zero is 1.0f)
    float currentPeak = processorRef.inputPeakLevel.load();

    if (currentPeak >= 0.95f)
        ledDecay = 1.0f; // Flash bright!
    else
        ledDecay *= 0.85f; // Fade out smoothly

    peakLed.setBrightness(ledDecay);
}