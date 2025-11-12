#include "PluginEditor.h"

PluginEditor::PluginEditor (PluginProcessor& p)
    : AudioProcessorEditor (&p), processorRef (p)
{
    juce::ignoreUnused (processorRef);

    addAndMakeVisible (inspectButton);

    // this chunk of code instantiates and opens the melatonin inspector
    inspectButton.onClick = [&] {
        if (!inspector)
        {
            inspector = std::make_unique<melatonin::Inspector> (*this);
            inspector->onClose = [this]() { inspector.reset(); };
        }

        inspector->setVisible (true);
    };

    addAndMakeVisible(delayTimeSlider);
    delayTimeSlider.setSliderStyle(juce::Slider::Rotary);
    delayTimeSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);

    // Attach the slider to the parameter
    delayTimeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.parameters, "delayTime", delayTimeSlider
    );
    addAndMakeVisible(feedbackSlider);
    feedbackSlider.setSliderStyle(juce::Slider::Rotary);
    feedbackSlider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);

    feedbackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processorRef.parameters, "feedback", feedbackSlider
    );
    addAndMakeVisible(head1Button);
    addAndMakeVisible(head2Button);
    addAndMakeVisible(head3Button);

    head1Button.onClick = [this] { processorRef.headEnabled[0] = head1Button.getToggleState(); };
    head2Button.onClick = [this] { processorRef.headEnabled[1] = head2Button.getToggleState(); };
    head3Button.onClick = [this] { processorRef.headEnabled[2] = head3Button.getToggleState(); };
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize (400, 300);
}

PluginEditor::~PluginEditor()
{
}

void PluginEditor::paint (juce::Graphics& g)
{
    // (Our component is opaque, so we must completely fill the background with a solid colour)
    g.fillAll (getLookAndFeel().findColour (juce::ResizableWindow::backgroundColourId));

    auto area = getLocalBounds();
    g.setColour (juce::Colours::white);
    g.setFont (16.0f);
    auto helloWorld = juce::String ("Bonjour from ") + PRODUCT_NAME_WITHOUT_VERSION + " v0.0.3" + " running in " + CMAKE_BUILD_TYPE;
    g.drawText (helloWorld, area.removeFromTop (150), juce::Justification::top, false);
}

void PluginEditor::resized()
{
    juce::FlexBox flex;
    flex.flexDirection = juce::FlexBox::Direction::column;
    flex.justifyContent = juce::FlexBox::JustifyContent::spaceAround;

    // Top row: sliders
    juce::FlexBox sliders;
    sliders.flexDirection = juce::FlexBox::Direction::row;
    sliders.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    sliders.items.add(juce::FlexItem(delayTimeSlider).withFlex(1.0f));
    sliders.items.add(juce::FlexItem(feedbackSlider).withFlex(1.0f));

    flex.items.add(juce::FlexItem(sliders).withHeight(150));

    // Bottom row: head buttons
    juce::FlexBox heads;
    heads.flexDirection = juce::FlexBox::Direction::row;
    heads.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    heads.items.add(juce::FlexItem(head1Button).withFlex(1.0f));
    heads.items.add(juce::FlexItem(head2Button).withFlex(1.0f));
    heads.items.add(juce::FlexItem(head3Button).withFlex(1.0f));

    flex.items.add(juce::FlexItem(heads).withHeight(50));

    // Bottom row: inspect button
    flex.items.add(juce::FlexItem(inspectButton).withHeight(40));

    flex.performLayout(getLocalBounds().reduced(10));
}
