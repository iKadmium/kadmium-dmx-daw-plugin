#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
ColorPreviewComponent::ColorPreviewComponent()
{
    setSize(200, 200);
}

void ColorPreviewComponent::paint(juce::Graphics &g)
{
    // Create color from HSB values
    auto color = juce::Colour::fromHSV(currentHue / 360.0f, currentSaturation / 100.0f,
                                       currentBrightness / 100.0f, 1.0f);

    // Fill the square with the color
    g.setColour(color);
    g.fillRect(getLocalBounds().reduced(2));

    // Draw black border
    g.setColour(juce::Colours::black);
    g.drawRect(getLocalBounds().reduced(2), 2);
}

void ColorPreviewComponent::setHSB(float hue, float saturation, float brightness)
{
    currentHue = hue;
    currentSaturation = saturation;
    currentBrightness = brightness;
    repaint();
}

//==============================================================================
KadmiumDMXAudioProcessorEditor::KadmiumDMXAudioProcessorEditor(KadmiumDMXAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Set up the color preview
    addAndMakeVisible(colorPreview);

    // Set up the toggle button
    toggleSlidersButton.setButtonText("Hide Controls");
    toggleSlidersButton.onClick = [this]()
    {
        slidersVisible = !slidersVisible;
        toggleSlidersButton.setButtonText(slidersVisible ? "Hide Controls" : "Show Controls");
        resized();
    };
    addAndMakeVisible(toggleSlidersButton);

    // Set up group selection dropdown
    groupSelectionLabel.setText("Group:", juce::dontSendNotification);
    groupSelectionLabel.attachToComponent(&groupSelectionCombo, true);
    addAndMakeVisible(groupSelectionLabel);

    groupSelectionCombo.onChange = [this]()
    {
        int selectedId = groupSelectionCombo.getSelectedId();
        if (selectedId > 0)
        {
            juce::String groupId = juce::String(selectedId - 1); // Convert back to 0-based
            audioProcessor.setSelectedGroup(groupId);
        }
    };
    addAndMakeVisible(groupSelectionCombo);

    // Update group selection options
    updateGroupSelection();

    // Create parameter sliders dynamically
    createParameterSliders();

    // Start timer to update color preview
    startTimer(50); // Update at 20 FPS

    // Set the editor's size
    setSize(400, 550);
}

void KadmiumDMXAudioProcessorEditor::createParameterSliders()
{
    // Get all parameter definitions from the processor
    auto paramDefinitions = audioProcessor.getAllParameterDefinitions();
    auto &apvts = audioProcessor.getValueTreeState();

    // Clear any existing sliders
    parameterSliders.clear();

    // Create sliders for each parameter
    for (const auto &paramDef : paramDefinitions)
    {
        ParameterSlider paramSlider;

        // Set up slider
        paramSlider.slider->setSliderStyle(juce::Slider::LinearVertical);
        paramSlider.slider->setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 20);
        paramSlider.slider->setTextValueSuffix(paramDef.unit);
        addAndMakeVisible(*paramSlider.slider);

        // Set up label
        paramSlider.label->setText(paramDef.name, juce::dontSendNotification);
        paramSlider.label->setJustificationType(juce::Justification::centred);
        paramSlider.label->attachToComponent(paramSlider.slider.get(), false);
        addAndMakeVisible(*paramSlider.label);

        // Create attachment to the processor
        paramSlider.attachment.reset(new juce::AudioProcessorValueTreeState::SliderAttachment(
            apvts, paramDef.id, *paramSlider.slider));

        parameterSliders.push_back(std::move(paramSlider));
    }
}

KadmiumDMXAudioProcessorEditor::~KadmiumDMXAudioProcessorEditor()
{
    stopTimer();
}

//==============================================================================
void KadmiumDMXAudioProcessorEditor::paint(juce::Graphics &g)
{
    // Fill background
    g.fillAll(getLookAndFeel().findColour(juce::ResizableWindow::backgroundColourId));
}

void KadmiumDMXAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto margin = 10;

    // Toggle button at top
    toggleSlidersButton.setBounds(bounds.removeFromTop(30).reduced(margin));
    bounds.removeFromTop(margin);

    // Group selection dropdown
    auto groupArea = bounds.removeFromTop(30).reduced(margin);
    groupArea.removeFromLeft(60); // Space for label
    groupSelectionCombo.setBounds(groupArea);
    bounds.removeFromTop(margin);

    // Color preview in center-top
    auto previewSize = 200;
    auto previewBounds = juce::Rectangle<int>(previewSize, previewSize);
    previewBounds.setCentre(bounds.getCentreX(), bounds.getY() + previewSize / 2 + margin);
    colorPreview.setBounds(previewBounds);

    // Move bounds below the preview
    bounds.removeFromTop(previewSize + margin * 2);

    // Sliders at bottom (if visible)
    if (slidersVisible && !parameterSliders.empty())
    {
        auto sliderArea = bounds.reduced(margin);
        auto numSliders = static_cast<int>(parameterSliders.size());
        auto sliderWidth = numSliders > 0 ? (sliderArea.getWidth() - margin * (numSliders - 1)) / numSliders : 0;

        // Position sliders horizontally
        for (int i = 0; i < numSliders; ++i)
        {
            if (i > 0)
                sliderArea.removeFromLeft(margin);

            parameterSliders[i].slider->setBounds(sliderArea.removeFromLeft(sliderWidth));
            parameterSliders[i].slider->setVisible(true);
            parameterSliders[i].label->setVisible(true);
        }

        // Adjust window height to accommodate sliders
        if (getHeight() < 550)
            setSize(getWidth(), 550);
    }
    else
    {
        // Hide slider components
        for (auto &paramSlider : parameterSliders)
        {
            paramSlider.slider->setVisible(false);
            paramSlider.label->setVisible(false);
        }

        // Adjust window height to be more compact
        setSize(getWidth(), 320);
    }
}

void KadmiumDMXAudioProcessorEditor::timerCallback()
{
    // Update color preview with current parameter values
    auto &apvts = audioProcessor.getValueTreeState();

    // Look for HSB parameters dynamically
    float hue = 0.0f, saturation = 100.0f, brightness = 100.0f;

    // Try to find hue parameter
    auto hueParam = apvts.getRawParameterValue("hue");
    if (!hueParam)
    {
        // Try alternative names
        for (const auto &paramId : audioProcessor.getAllParameterIDs())
        {
            if (paramId.containsIgnoreCase("hue"))
            {
                hueParam = apvts.getRawParameterValue(paramId);
                break;
            }
        }
    }

    // Try to find saturation parameter
    auto saturationParam = apvts.getRawParameterValue("saturation");
    if (!saturationParam)
    {
        for (const auto &paramId : audioProcessor.getAllParameterIDs())
        {
            if (paramId.containsIgnoreCase("saturation"))
            {
                saturationParam = apvts.getRawParameterValue(paramId);
                break;
            }
        }
    }

    // Try to find brightness parameter
    auto brightnessParam = apvts.getRawParameterValue("brightness");
    if (!brightnessParam)
    {
        for (const auto &paramId : audioProcessor.getAllParameterIDs())
        {
            if (paramId.containsIgnoreCase("brightness"))
            {
                brightnessParam = apvts.getRawParameterValue(paramId);
                break;
            }
        }
    }

    if (hueParam)
        hue = *hueParam;
    if (saturationParam)
        saturation = *saturationParam;
    if (brightnessParam)
        brightness = *brightnessParam;

    colorPreview.setHSB(hue, saturation, brightness);
}

void KadmiumDMXAudioProcessorEditor::updateGroupSelection()
{
    groupSelectionCombo.clear();

    auto availableGroups = audioProcessor.getAvailableGroups();
    auto &midiMap = audioProcessor.getMidiMap();

    for (const auto &groupId : availableGroups)
    {
        juce::String groupName = midiMap.getGroupName(groupId);
        // Use 1-based IDs for ComboBox (0 means no selection)
        groupSelectionCombo.addItem(groupName, groupId.getIntValue() + 1);
    }

    // Select current group
    juce::String currentGroup = audioProcessor.getSelectedGroup();
    groupSelectionCombo.setSelectedId(currentGroup.getIntValue() + 1);
}
