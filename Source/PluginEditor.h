#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"

//==============================================================================
class KadmiumDMXAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    KadmiumDMXAudioProcessorEditor(KadmiumDMXAudioProcessor &);
    ~KadmiumDMXAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;

private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    KadmiumDMXAudioProcessor &audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KadmiumDMXAudioProcessorEditor)
};
