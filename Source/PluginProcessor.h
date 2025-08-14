#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MidiMap.h"
#include "MqttClient.h"

//==============================================================================
class KadmiumDMXAudioProcessor : public juce::AudioProcessor,
                                 public juce::Timer,
                                 public juce::AudioProcessorValueTreeState::Listener,
                                 public juce::ChangeBroadcaster
{
public:
    //==============================================================================
    KadmiumDMXAudioProcessor();
    ~KadmiumDMXAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    using AudioProcessor::processBlock;

    //==============================================================================
    juce::AudioProcessorEditor *createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    //==============================================================================
    // Parameter management
    juce::AudioProcessorValueTreeState &getValueTreeState() { return *apvts; }

    // Dynamic parameter definition structure
    struct ParameterDefinition
    {
        juce::String id;
        juce::String name;
        float minValue;
        float maxValue;
        float defaultValue;
        juce::String unit;

        ParameterDefinition() = default;
        ParameterDefinition(const juce::String &paramId, const juce::String &paramName,
                            float min, float max, float def, const juce::String &paramUnit)
            : id(paramId), name(paramName), minValue(min), maxValue(max),
              defaultValue(def), unit(paramUnit) {}
    };

    // Utility functions for parameter access
    float getParameterValue(const juce::String &parameterID) const;
    void setParameterValue(const juce::String &parameterID, float value);

    // Get all parameter IDs (for dynamic parameter management)
    juce::StringArray getAllParameterIDs() const;

    // Get parameter definition
    ParameterDefinition getParameterDefinition(const juce::String &parameterID) const;

    // Get all parameter definitions
    std::vector<ParameterDefinition> getAllParameterDefinitions() const;

    //==============================================================================
    // MIDI Map management
    const MidiMap &getMidiMap() const { return currentMidiMap; }
    juce::Result loadMidiMap(const juce::String &jsonString);
    juce::Result loadMidiMapFromFile(const juce::File &file);
    void loadMidiMapFromMqtt();
    juce::String serializeMidiMap() const;
    void createDefaultMidiMap();

    // Group selection
    juce::String getSelectedGroup() const;
    void setSelectedGroup(const juce::String &groupId);
    juce::StringArray getAvailableGroups() const;

    // MIDI output functionality
    void sendMidiCC(int channel, int ccNumber, int value);
    void sendAllParametersAsMidi();

    // MQTT functionality
    bool isMqttConnected() const;
    juce::String getMqttStatus() const;

private:
    //==============================================================================
    // Parameter management
    std::unique_ptr<juce::AudioProcessorValueTreeState> apvts;

    // Dynamic parameter definitions - preserves order from MIDI map
    std::vector<std::pair<juce::String, ParameterDefinition>> parameterDefinitions;

    // MIDI Map for group and attribute mapping
    MidiMap currentMidiMap;

    // Selected group for MIDI output
    juce::String selectedGroupId;

    // MIDI output buffer for sending CC messages
    juce::MidiBuffer midiOutputBuffer;

    // Timer for periodic MIDI output (every 5 seconds)
    static constexpr int MIDI_BLAST_INTERVAL_MS = 5000; // 5 seconds

    // MQTT client for networked DMX control
    MqttClient mqttClient;

    // Initialize parameter definitions
    void initializeParameterDefinitions();

    // Recreate parameters from MIDI map attributes
    void recreateParametersFromMidiMap();

    // Create parameter layout from definitions
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // Timer callback for periodic MIDI output
    void timerCallback() override;

    // Parameter change callback for MIDI output
    void parameterChanged(const juce::String &parameterID, float newValue) override;

    // MQTT message handler
    void handleMqttMessage(const juce::String &topic, const juce::String &message);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KadmiumDMXAudioProcessor)
};
