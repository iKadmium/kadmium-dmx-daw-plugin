#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
class ColorPreviewComponent : public juce::Component
{
public:
    ColorPreviewComponent();

    void paint(juce::Graphics &g) override;
    void setHSB(float hue, float saturation, float brightness);

private:
    float currentHue = 0.0f;
    float currentSaturation = 1.0f;
    float currentBrightness = 1.0f;
};

//==============================================================================
class KadmiumDMXAudioProcessorEditor : public juce::AudioProcessorEditor,
                                       public juce::Timer,
                                       public juce::ChangeListener
{
public:
    KadmiumDMXAudioProcessorEditor(KadmiumDMXAudioProcessor &);
    ~KadmiumDMXAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;
    void timerCallback() override;

    // ChangeListener callback
    void changeListenerCallback(juce::ChangeBroadcaster *source) override;

private:
    // Helper function to convert HSB to RGB
    juce::Colour hsbToColour(float hue, float saturation, float brightness);

    // Component layout
    void layoutComponents();

    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    KadmiumDMXAudioProcessor &audioProcessor;

    // UI Components
    ColorPreviewComponent colorPreview;

    juce::TextButton toggleSlidersButton;
    bool slidersVisible = true;

    // MQTT functionality
    juce::TextButton loadMidiMapButton;
    juce::Label mqttStatusLabel;

    // Group selection dropdown
    juce::ComboBox groupSelectionCombo;
    juce::Label groupSelectionLabel;

    // Dynamic parameter sliders and attachments
    struct ParameterSlider
    {
        std::unique_ptr<juce::Slider> slider;
        std::unique_ptr<juce::Label> label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

        ParameterSlider()
            : slider(std::make_unique<juce::Slider>()), label(std::make_unique<juce::Label>())
        {
        }
    };

    std::vector<ParameterSlider> parameterSliders;

    // Create sliders dynamically based on processor parameters
    void createParameterSliders();

    // Update group selection dropdown
    void updateGroupSelection();

    // Recreate UI when MIDI map changes
    void recreateUIFromMidiMap();

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KadmiumDMXAudioProcessorEditor)
};
