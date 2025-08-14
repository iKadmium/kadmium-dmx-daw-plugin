#pragma once

#include <juce_core/juce_core.h>
#include <vector>
#include <utility>

//==============================================================================
/**
 * MIDI Map data structures for DMX light control mapping
 */

struct MidiMap
{
    // Group ID to name mapping (e.g., "0" -> "Vocalist") - preserves order
    std::vector<std::pair<juce::String, juce::String>> groups;

    // Attribute ID to name mapping (e.g., "1" -> "Hue") - preserves order
    std::vector<std::pair<juce::String, juce::String>> attributes;

    // Default constructor
    MidiMap() = default;

    // Utility methods
    bool hasGroup(const juce::String &groupId) const;
    bool hasAttribute(const juce::String &attributeId) const;
    juce::String getGroupName(const juce::String &groupId) const;
    juce::String getAttributeName(const juce::String &attributeId) const;

    // Get all group IDs
    juce::StringArray getAllGroupIds() const;

    // Get all attribute IDs
    juce::StringArray getAllAttributeIds() const;

    // Validation
    bool isValid() const;

    // Debug output
    juce::String toString() const;
};

//==============================================================================
/**
 * MIDI Map JSON serialization/deserialization
 */
class MidiMapSerializer
{
public:
    // Deserialize from JSON string
    static juce::Result deserialize(const juce::String &jsonString, MidiMap &midiMap);

    // Deserialize from JSON var
    static juce::Result deserialize(const juce::var &jsonVar, MidiMap &midiMap);

    // Serialize to JSON string
    static juce::String serialize(const MidiMap &midiMap);

    // Serialize to JSON var
    static juce::var serializeToVar(const MidiMap &midiMap);

    // Load from file
    static juce::Result loadFromFile(const juce::File &file, MidiMap &midiMap);

    // Save to file
    static juce::Result saveToFile(const juce::File &file, const MidiMap &midiMap);

private:
    static juce::Result parseGroups(const juce::var &groupsVar, std::vector<std::pair<juce::String, juce::String>> &groups);
    static juce::Result parseAttributes(const juce::var &attributesVar, std::vector<std::pair<juce::String, juce::String>> &attributes);
    static juce::var createGroupsVar(const std::vector<std::pair<juce::String, juce::String>> &groups);
    static juce::var createAttributesVar(const std::vector<std::pair<juce::String, juce::String>> &attributes);
};
