#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
KadmiumDMXAudioProcessor::KadmiumDMXAudioProcessor()
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                         ),
      selectedGroupId("0") // Default to group 0
{
    // Initialize parameter definitions first
    initializeParameterDefinitions();

    // Initialize default MIDI map
    createDefaultMidiMap();

    // Then create the APVTS with the layout
    apvts.reset(new juce::AudioProcessorValueTreeState(*this, nullptr, "Parameters", createParameterLayout()));

    // Register as listener for parameter changes
    for (const auto &paramPair : parameterDefinitions)
    {
        apvts->addParameterListener(paramPair.second.id, this);
    }

    // Start the MIDI blast timer (every 5 seconds)
    startTimer(MIDI_BLAST_INTERVAL_MS);

    // Initialize MQTT client with callbacks
    mqttClient.setConnectionCallback([this](bool connected, const juce::String &error)
                                     {
        if (connected) {
            DBG("MQTT connected successfully");
            // Subscribe to DMX command topics
            mqttClient.subscribe("dmx/+/command");
        } else {
            DBG("MQTT connection failed: " + error);
        } });

    mqttClient.setMessageCallback([this](const juce::String &topic, const juce::String &message)
                                  {
        // Handle incoming DMX commands
        handleMqttMessage(topic, message); });
}

KadmiumDMXAudioProcessor::~KadmiumDMXAudioProcessor()
{
    stopTimer();

    // Remove parameter listeners
    if (apvts)
    {
        for (const auto &paramPair : parameterDefinitions)
        {
            apvts->removeParameterListener(paramPair.second.id, this);
        }
    }
}

//==============================================================================
void KadmiumDMXAudioProcessor::initializeParameterDefinitions()
{
    // Define our current parameters - this is where you'd modify to add/remove parameters
    parameterDefinitions.push_back({"hue", ParameterDefinition("hue", "Hue", 0.0f, 360.0f, 0.0f, juce::String::fromUTF8(u8"°"))});
    parameterDefinitions.push_back({"saturation", ParameterDefinition("saturation", "Saturation", 0.0f, 100.0f, 100.0f, "%")});
    parameterDefinitions.push_back({"brightness", ParameterDefinition("brightness", "Brightness", 0.0f, 100.0f, 100.0f, "%")});

    // You could easily add more parameters here:
    // parameterDefinitions.push_back({"intensity", ParameterDefinition("intensity", "Intensity", 0.0f, 100.0f, 100.0f, "%")});
    // parameterDefinitions.push_back({"strobe", ParameterDefinition("strobe", "Strobe Rate", 0.0f, 20.0f, 0.0f, "Hz")});
}

void KadmiumDMXAudioProcessor::recreateParametersFromMidiMap()
{
    // Clear existing parameter definitions
    parameterDefinitions.clear();

    // Create parameters from MIDI map attributes
    for (const auto &attributePair : currentMidiMap.attributes)
    {
        const juce::String &attributeId = attributePair.first;
        const juce::String &attributeName = attributePair.second;

        // Determine parameter range based on attribute name
        float minValue = 0.0f;
        float maxValue = 100.0f;
        float defaultValue = 0.0f;
        juce::String unit = "";

        if (attributeName.containsIgnoreCase("hue"))
        {
            maxValue = 360.0f;
            unit = juce::String::fromUTF8(u8"°");
        }
        else if (attributeName.containsIgnoreCase("saturation") ||
                 attributeName.containsIgnoreCase("brightness") ||
                 attributeName.containsIgnoreCase("intensity"))
        {
            maxValue = 100.0f;
            defaultValue = 100.0f;
            unit = "%";
        }
        else if (attributeName.containsIgnoreCase("strobe"))
        {
            maxValue = 20.0f;
            unit = "Hz";
        }

        // Create parameter with lowercase ID for consistency
        juce::String paramId = attributeName.toLowerCase().removeCharacters(" ");
        parameterDefinitions.push_back({paramId, ParameterDefinition(
                                                     paramId, attributeName, minValue, maxValue, defaultValue, unit)});
    }

    // Recreate APVTS with new parameters
    apvts.reset(new juce::AudioProcessorValueTreeState(*this, nullptr, "Parameters", createParameterLayout()));

    // Re-register parameter listeners for the new parameters
    for (const auto &paramPair : parameterDefinitions)
    {
        apvts->addParameterListener(paramPair.second.id, this);
    }

    // Notify listeners (including the editor) that the MIDI map has changed
    sendChangeMessage();
}

juce::AudioProcessorValueTreeState::ParameterLayout KadmiumDMXAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Create parameters dynamically from our definitions
    for (const auto &paramPair : parameterDefinitions)
    {
        const auto &def = paramPair.second;

        layout.add(std::make_unique<juce::AudioParameterFloat>(
            def.id,
            def.name,
            juce::NormalisableRange<float>(def.minValue, def.maxValue, 1.0f),
            def.defaultValue,
            juce::AudioParameterFloatAttributes().withLabel(def.unit)));
    }

    return layout;
}

//==============================================================================
float KadmiumDMXAudioProcessor::getParameterValue(const juce::String &parameterID) const
{
    auto *param = apvts->getParameter(parameterID);
    if (param != nullptr)
        return param->getValue();
    return 0.0f;
}

void KadmiumDMXAudioProcessor::setParameterValue(const juce::String &parameterID, float value)
{
    auto *param = apvts->getParameter(parameterID);
    if (param != nullptr)
    {
        param->setValueNotifyingHost(value);

        // Send MIDI CC when parameter changes
        if (currentMidiMap.hasGroup(selectedGroupId))
        {
            // Find corresponding attribute ID in MIDI map
            for (const auto &attributePair : currentMidiMap.attributes)
            {
                const juce::String &attributeId = attributePair.first;
                const juce::String &attributeName = attributePair.second;

                // Match parameter to attribute (case-insensitive)
                if (parameterID.containsIgnoreCase(attributeName) ||
                    attributeName.toLowerCase().removeCharacters(" ") == parameterID)
                {
                    // Get parameter definition for value conversion
                    auto paramDef = getParameterDefinition(parameterID);

                    // Normalize to 0-1 range, then scale to 0-127
                    float normalizedValue = (value - paramDef.minValue) / (paramDef.maxValue - paramDef.minValue);
                    int midiValue = juce::roundToInt(normalizedValue * 127.0f);

                    // Send MIDI CC
                    int midiChannel = selectedGroupId.getIntValue() + 1; // Convert to 1-based MIDI channel
                    int ccNumber = attributeId.getIntValue();
                    sendMidiCC(midiChannel, ccNumber, midiValue);
                    break;
                }
            }
        }
    }
}

juce::StringArray KadmiumDMXAudioProcessor::getAllParameterIDs() const
{
    juce::StringArray ids;
    for (const auto &paramPair : parameterDefinitions)
    {
        ids.add(paramPair.first);
    }
    return ids;
}

KadmiumDMXAudioProcessor::ParameterDefinition KadmiumDMXAudioProcessor::getParameterDefinition(const juce::String &parameterID) const
{
    for (const auto &paramPair : parameterDefinitions)
    {
        if (paramPair.first == parameterID)
            return paramPair.second;
    }

    // Return empty definition if not found
    return ParameterDefinition();
}

std::vector<KadmiumDMXAudioProcessor::ParameterDefinition> KadmiumDMXAudioProcessor::getAllParameterDefinitions() const
{
    std::vector<ParameterDefinition> definitions;
    for (const auto &paramPair : parameterDefinitions)
    {
        definitions.push_back(paramPair.second);
    }
    return definitions;
}

//==============================================================================
const juce::String KadmiumDMXAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool KadmiumDMXAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool KadmiumDMXAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool KadmiumDMXAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double KadmiumDMXAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int KadmiumDMXAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int KadmiumDMXAudioProcessor::getCurrentProgram()
{
    return 0;
}

void KadmiumDMXAudioProcessor::setCurrentProgram(int index)
{
    juce::ignoreUnused(index);
}

const juce::String KadmiumDMXAudioProcessor::getProgramName(int index)
{
    juce::ignoreUnused(index);
    return {};
}

void KadmiumDMXAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
    juce::ignoreUnused(index, newName);
}

//==============================================================================
void KadmiumDMXAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    // initialisation that you need..
    juce::ignoreUnused(sampleRate, samplesPerBlock);
}

void KadmiumDMXAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

bool KadmiumDMXAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
#if JucePlugin_IsMidiEffect
    juce::ignoreUnused(layouts);
    return true;
#else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono() && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
#if !JucePlugin_IsSynth
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
        return false;
#endif

    return true;
#endif
}

void KadmiumDMXAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                            juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear unused outputs
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    // Add any pending MIDI output messages
    midiMessages.addEvents(midiOutputBuffer, 0, buffer.getNumSamples(), 0);
    midiOutputBuffer.clear();

    // Audio processing (if needed)
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto *channelData = buffer.getWritePointer(channel);
        juce::ignoreUnused(channelData);
        // Audio processing would go here if needed
    }
}

//==============================================================================
bool KadmiumDMXAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *KadmiumDMXAudioProcessor::createEditor()
{
    return new KadmiumDMXAudioProcessorEditor(*this);
}

//==============================================================================
void KadmiumDMXAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    // Save the parameter state
    auto state = apvts->copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void KadmiumDMXAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    // Restore the parameter state
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));

    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts->state.getType()))
            apvts->replaceState(juce::ValueTree::fromXml(*xmlState));
}

//==============================================================================
// MIDI Map management

juce::Result KadmiumDMXAudioProcessor::loadMidiMap(const juce::String &jsonString)
{
    MidiMap newMidiMap;
    auto result = MidiMapSerializer::deserialize(jsonString, newMidiMap);

    if (result.wasOk())
    {
        currentMidiMap = std::move(newMidiMap);
        recreateParametersFromMidiMap(); // Recreate parameters from new MIDI map
        DBG("MIDI Map loaded successfully:");
        DBG(currentMidiMap.toString());
    }
    else
    {
        DBG("Failed to load MIDI Map: " + result.getErrorMessage());
    }

    return result;
}

juce::Result KadmiumDMXAudioProcessor::loadMidiMapFromFile(const juce::File &file)
{
    MidiMap newMidiMap;
    auto result = MidiMapSerializer::loadFromFile(file, newMidiMap);

    if (result.wasOk())
    {
        currentMidiMap = std::move(newMidiMap);
        recreateParametersFromMidiMap(); // Recreate parameters from new MIDI map
        DBG("MIDI Map loaded from file: " + file.getFullPathName());
        DBG(currentMidiMap.toString());
    }
    else
    {
        DBG("Failed to load MIDI Map from file: " + result.getErrorMessage());
    }

    return result;
}

void KadmiumDMXAudioProcessor::loadMidiMapFromMqtt()
{
    DBG("Loading MIDI map from MQTT...");

    // Set up MQTT connection callbacks
    mqttClient.setConnectionCallback([this](bool connected, const juce::String &error)
                                     {
        if (connected)
        {
            DBG("MQTT connected successfully");
            // Subscribe to the config/midi_map topic
            mqttClient.subscribe("config/midi_map");
        }
        else
        {
            DBG("MQTT connection failed: " + error);
        } });

    mqttClient.setMessageCallback([this](const juce::String &topic, const juce::String &message)
                                  {
        if (topic == "config/midi_map")
        {
            DBG("Received MIDI map from MQTT: " + message);
            auto result = loadMidiMap(message);
            if (result.wasOk())
            {
                DBG("MIDI map loaded successfully from MQTT");
            }
            else
            {
                DBG("Failed to load MIDI map from MQTT: " + result.getErrorMessage());
            }
        } });

    // Connect to localhost MQTT broker
    mqttClient.connect("tcp://localhost:1883", "KadmiumDMXPlugin");
}

juce::String KadmiumDMXAudioProcessor::serializeMidiMap() const
{
    return MidiMapSerializer::serialize(currentMidiMap);
}

void KadmiumDMXAudioProcessor::createDefaultMidiMap()
{
    // Create the default MIDI map matching your example
    currentMidiMap.groups.clear();
    currentMidiMap.attributes.clear();

    // Groups - in order
    currentMidiMap.groups.push_back({"0", "Vocalist"});
    currentMidiMap.groups.push_back({"1", "Guitarist"});
    currentMidiMap.groups.push_back({"2", "Bassist"});
    currentMidiMap.groups.push_back({"3", "Drummer"});
    currentMidiMap.groups.push_back({"4", "Rear"});

    // Attributes - in order
    currentMidiMap.attributes.push_back({"1", "Hue"});
    currentMidiMap.attributes.push_back({"2", "Saturation"});
    currentMidiMap.attributes.push_back({"3", "Brightness"});

    DBG("Default MIDI Map created:");
    DBG(currentMidiMap.toString());
}

//==============================================================================
// Group selection methods
juce::String KadmiumDMXAudioProcessor::getSelectedGroup() const
{
    return selectedGroupId;
}

void KadmiumDMXAudioProcessor::setSelectedGroup(const juce::String &groupId)
{
    if (currentMidiMap.hasGroup(groupId))
    {
        selectedGroupId = groupId;
        DBG("Selected group: " + groupId + " (" + currentMidiMap.getGroupName(groupId) + ")");
    }
}

juce::StringArray KadmiumDMXAudioProcessor::getAvailableGroups() const
{
    return currentMidiMap.getAllGroupIds();
}

//==============================================================================
// MIDI output methods
void KadmiumDMXAudioProcessor::sendMidiCC(int channel, int ccNumber, int value)
{
    // Clamp values to valid MIDI ranges
    channel = juce::jlimit(1, 16, channel);
    ccNumber = juce::jlimit(0, 127, ccNumber);
    value = juce::jlimit(0, 127, value);

    // Create MIDI CC message
    auto midiMessage = juce::MidiMessage::controllerEvent(channel, ccNumber, value);

    // Add to output buffer (will be sent in next processBlock call)
    midiOutputBuffer.addEvent(midiMessage, 0);

    DBG("Sending MIDI CC: Channel " + juce::String(channel) +
        ", CC " + juce::String(ccNumber) +
        ", Value " + juce::String(value));
}

void KadmiumDMXAudioProcessor::sendAllParametersAsMidi()
{
    if (!currentMidiMap.hasGroup(selectedGroupId))
        return;

    // Get the MIDI channel for the selected group
    int midiChannel = selectedGroupId.getIntValue() + 1; // Convert to 1-based MIDI channel

    // Send all parameters as MIDI CC
    for (const auto &paramPair : parameterDefinitions)
    {
        const juce::String &paramId = paramPair.first;

        // Find corresponding attribute ID in MIDI map
        for (const auto &attributePair : currentMidiMap.attributes)
        {
            const juce::String &attributeId = attributePair.first;
            const juce::String &attributeName = attributePair.second;

            // Match parameter to attribute (case-insensitive)
            if (paramId.containsIgnoreCase(attributeName) ||
                attributeName.toLowerCase().removeCharacters(" ") == paramId)
            {
                // Get parameter value and convert to MIDI range (0-127)
                float paramValue = getParameterValue(paramId);
                auto paramDef = getParameterDefinition(paramId);

                // Normalize to 0-1 range, then scale to 0-127
                float normalizedValue = (paramValue - paramDef.minValue) / (paramDef.maxValue - paramDef.minValue);
                int midiValue = juce::roundToInt(normalizedValue * 127.0f);

                // Send MIDI CC
                int ccNumber = attributeId.getIntValue();
                sendMidiCC(midiChannel, ccNumber, midiValue);
                break;
            }
        }
    }
}

//==============================================================================
// Timer callback for periodic MIDI output
void KadmiumDMXAudioProcessor::timerCallback()
{
    // Send all parameters every 5 seconds
    sendAllParametersAsMidi();
}

//==============================================================================
// Parameter change callback for MIDI output
void KadmiumDMXAudioProcessor::parameterChanged(const juce::String &parameterID, float newValue)
{
    // Send MIDI CC when parameter changes
    if (!currentMidiMap.hasGroup(selectedGroupId))
        return;

    // Find corresponding attribute ID in MIDI map
    for (const auto &attributePair : currentMidiMap.attributes)
    {
        const juce::String &attributeId = attributePair.first;
        const juce::String &attributeName = attributePair.second;

        // Match parameter to attribute (case-insensitive)
        if (parameterID.containsIgnoreCase(attributeName) ||
            attributeName.toLowerCase().removeCharacters(" ") == parameterID)
        {
            // Get parameter definition for value conversion
            auto paramDef = getParameterDefinition(parameterID);

            // Get the actual current value from the parameter
            auto *param = apvts->getParameter(parameterID);
            if (param)
            {
                float currentValue = param->getValue();
                // Convert normalized value back to actual range
                float actualValue = paramDef.minValue + currentValue * (paramDef.maxValue - paramDef.minValue);

                // Normalize to 0-1 range, then scale to 0-127
                float normalizedValue = (actualValue - paramDef.minValue) / (paramDef.maxValue - paramDef.minValue);
                int midiValue = juce::roundToInt(normalizedValue * 127.0f);

                // Send MIDI CC
                int midiChannel = selectedGroupId.getIntValue() + 1; // Convert to 1-based MIDI channel
                int ccNumber = attributeId.getIntValue();
                sendMidiCC(midiChannel, ccNumber, midiValue);

                // Publish to MQTT if connected
                if (mqttClient.getConnectionStatus())
                {
                    juce::String groupName = currentMidiMap.getGroupName(selectedGroupId);
                    juce::String topic = "dmx/" + groupName + "/" + attributeName;
                    mqttClient.publish(topic, juce::String(actualValue, 2));
                }

                DBG("Parameter '" + parameterID + "' changed to " + juce::String(actualValue) +
                    " -> MIDI CC Ch" + juce::String(midiChannel) + " CC" + juce::String(ccNumber) +
                    " Val" + juce::String(midiValue));
                break;
            }
        }
    }
}

//==============================================================================
//==============================================================================
void KadmiumDMXAudioProcessor::handleMqttMessage(const juce::String &topic, const juce::String &message)
{
    DBG("MQTT message received on '" + topic + "': " + message);

    // TODO: Add MQTT message handling logic here
    // For example, handle incoming DMX control commands
}

bool KadmiumDMXAudioProcessor::isMqttConnected() const
{
    return mqttClient.getConnectionStatus();
}

juce::String KadmiumDMXAudioProcessor::getMqttStatus() const
{
    if (mqttClient.getConnectionStatus())
    {
        return "MQTT: Connected";
    }
    else
    {
        return "MQTT: Disconnected";
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new KadmiumDMXAudioProcessor();
}
