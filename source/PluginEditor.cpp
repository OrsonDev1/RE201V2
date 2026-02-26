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

    // FIX: Set initial toggle state to match the processor defaults (true)
    head1Button.setToggleState(processorRef.headEnabled[0], juce::dontSendNotification);
    head2Button.setToggleState(processorRef.headEnabled[1], juce::dontSendNotification);
    head3Button.setToggleState(processorRef.headEnabled[2], juce::dontSendNotification);
    head1Button.setColour(juce::ToggleButton::textColourId, juce::Colours::black);
    head2Button.setColour(juce::ToggleButton::textColourId, juce::Colours::black);
    head3Button.setColour(juce::ToggleButton::textColourId, juce::Colours::black);

    head1Button.onClick = [this] { processorRef.headEnabled[0] = head1Button.getToggleState(); };
    head2Button.onClick = [this] { processorRef.headEnabled[1] = head2Button.getToggleState(); };
    head3Button.onClick = [this] { processorRef.headEnabled[2] = head3Button.getToggleState(); };
    head1Button.setColour(juce::ToggleButton::tickColourId, juce::Colours::black);
    head2Button.setColour(juce::ToggleButton::tickColourId, juce::Colours::black);
    head3Button.setColour(juce::ToggleButton::tickColourId, juce::Colours::black);

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

    // FIX: Move line to X=170, and shorten the bottom (faceplate.getBottom() - 130)
    // so it cleanly stops right above the master mixer strip.
    g.drawLine(170.0f, faceplate.getY() + 10.0f, 170.0f, faceplate.getBottom() - 130.0f, 1.0f);

    // 4. Title
    g.setColour(juce::Colours::black);
    g.setFont(24.0f);
    g.drawText("SPACE ECHO RE-201 Version 0.1.5", area.removeFromTop(40), juce::Justification::centred, false);
}

void PluginEditor::resized()
{
    auto area = getLocalBounds().reduced(20);
    area.removeFromTop(40); // Skip title

    // --- BOTTOM MIXER STRIP (Master Section) ---
    auto bottomStrip = area.removeFromBottom(120);

    // FIX: Divide by 5 to make room for Input Gain
    int mixKnobWidth = bottomStrip.getWidth() / 5;

    // 1. INPUT GAIN & LED (Col 0)
    int col0 = bottomStrip.getX();
    inputGainLabel.setBounds(col0, bottomStrip.getY(), mixKnobWidth, 20);
    inputGainSlider.setBounds(col0, bottomStrip.getY() + 20, mixKnobWidth, 80);
    // Place LED near the top right of the knob
    peakLed.setBounds(col0 + mixKnobWidth - 30, bottomStrip.getY() + 10, 15, 15);

    // 2. ECHO MIX (Col 1)
    int col1 = bottomStrip.getX() + mixKnobWidth;
    echoMixLabel.setBounds(col1, bottomStrip.getY(), mixKnobWidth, 20);
    echoMixSlider.setBounds(col1, bottomStrip.getY() + 20, mixKnobWidth, 80);

    // 3. REVERB MIX (Col 2)
    int col2 = bottomStrip.getX() + mixKnobWidth * 2;
    reverbMixLabel.setBounds(col2, bottomStrip.getY(), mixKnobWidth, 20);
    reverbMixSlider.setBounds(col2, bottomStrip.getY() + 20, mixKnobWidth, 80);

    // Load and Reset buttons under Reverb Mix
    int buttonY = reverbMixSlider.getBottom() + 5;
    int loadBtnWidth = mixKnobWidth - 40;
    loadIRButton.setBounds(col2 + 5, buttonY, loadBtnWidth, 20);
    resetIRButton.setBounds(loadIRButton.getRight() + 5, buttonY, 25, 20);

    // 4. MASTER MIX (Col 3)
    int col3 = bottomStrip.getX() + mixKnobWidth * 3;
    masterMixLabel.setBounds(col3, bottomStrip.getY(), mixKnobWidth, 20);
    masterMixSlider.setBounds(col3, bottomStrip.getY() + 20, mixKnobWidth, 80);

    // 5. MASTER GAIN (Col 4)
    int col4 = bottomStrip.getX() + mixKnobWidth * 4;
    masterGainLabel.setBounds(col4, bottomStrip.getY(), mixKnobWidth, 20);
    masterGainSlider.setBounds(col4, bottomStrip.getY() + 20, mixKnobWidth, 80);

    // --- LEFT COLUMN (Heads) ---
    auto leftCol = area.removeFromLeft(150);
    int buttonHeight = 40;
    int startY = leftCol.getY() + 40;

    head1Button.setBounds(leftCol.getX() + 10, startY, 100, buttonHeight);
    head2Button.setBounds(leftCol.getX() + 10, startY + 50, 100, buttonHeight);
    head3Button.setBounds(leftCol.getX() + 10, startY + 100, 100, buttonHeight);

    // --- MAIN EFFECTS GRID (Right Side) ---
    int knobSize = 90;
    int spacing = 20; // Horizontal spacing
    int vSpacing = 10; // Vertical spacing between rows

    // Starting coordinates for the grid
    int gridStartX = area.getX() + 20;
    int gridStartY = area.getY() + 10;

    // --- ROW 1: Delay | Feedback | Saturation ---
    int row1Y = gridStartY;

    delayLabel.setBounds(gridStartX, row1Y, knobSize, 20);
    delayTimeSlider.setBounds(gridStartX, row1Y + 20, knobSize, knobSize);

    feedbackLabel.setBounds(gridStartX + knobSize + spacing, row1Y, knobSize, 20);
    feedbackSlider.setBounds(gridStartX + knobSize + spacing, row1Y + 20, knobSize, knobSize);

    // Extra width for Saturation label so it doesn't squish
    int satX = gridStartX + (knobSize + spacing) * 2;
    saturationLabel.setBounds(satX - 10, row1Y, knobSize + 20, 20);
    saturationSlider.setBounds(satX, row1Y + 20, knobSize, knobSize);

    // --- ROW 2: Wow | Flutter ---
    int row2Y = row1Y + knobSize + 20 + vSpacing;

    wowLabel.setBounds(gridStartX, row2Y, knobSize, 20);
    wowSlider.setBounds(gridStartX, row2Y + 20, knobSize, knobSize);

    flutterLabel.setBounds(gridStartX + knobSize + spacing, row2Y, knobSize, 20);
    flutterSlider.setBounds(gridStartX + knobSize + spacing, row2Y + 20, knobSize, knobSize);

    // --- ROW 3: BASS & TREBLE ---
    int row3Y = row2Y + knobSize + 20 + vSpacing;

    bassLabel.setBounds(gridStartX, row3Y, knobSize, 20);
    bassSlider.setBounds(gridStartX, row3Y + 20, knobSize, knobSize);

    trebleLabel.setBounds(gridStartX + knobSize + spacing, row3Y, knobSize, 20);
    trebleSlider.setBounds(gridStartX + knobSize + spacing, row3Y + 20, knobSize, knobSize);
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