#include "MidiMap.h"

//==============================================================================
// MidiMap implementation

bool MidiMap::hasGroup(const juce::String &groupId) const
{
    for (const auto &pair : groups)
    {
        if (pair.first == groupId)
            return true;
    }
    return false;
}

bool MidiMap::hasAttribute(const juce::String &attributeId) const
{
    for (const auto &pair : attributes)
    {
        if (pair.first == attributeId)
            return true;
    }
    return false;
}

juce::String MidiMap::getGroupName(const juce::String &groupId) const
{
    for (const auto &pair : groups)
    {
        if (pair.first == groupId)
            return pair.second;
    }
    return juce::String();
}

juce::String MidiMap::getAttributeName(const juce::String &attributeId) const
{
    for (const auto &pair : attributes)
    {
        if (pair.first == attributeId)
            return pair.second;
    }
    return juce::String();
}

juce::StringArray MidiMap::getAllGroupIds() const
{
    juce::StringArray ids;
    for (const auto &pair : groups)
    {
        ids.add(pair.first);
    }
    return ids;
}

juce::StringArray MidiMap::getAllAttributeIds() const
{
    juce::StringArray ids;
    for (const auto &pair : attributes)
    {
        ids.add(pair.first);
    }
    return ids;
}

bool MidiMap::isValid() const
{
    return !groups.empty() && !attributes.empty();
}

juce::String MidiMap::toString() const
{
    juce::String result = "MidiMap:\n";

    result += "Groups:\n";
    for (const auto &pair : groups)
    {
        result += "  " + pair.first + " -> " + pair.second + "\n";
    }

    result += "Attributes:\n";
    for (const auto &pair : attributes)
    {
        result += "  " + pair.first + " -> " + pair.second + "\n";
    }

    return result;
}

//==============================================================================
// MidiMapSerializer implementation

juce::Result MidiMapSerializer::deserialize(const juce::String &jsonString, MidiMap &midiMap)
{
    auto parseResult = juce::JSON::parse(jsonString);

    if (!parseResult.isObject())
        return juce::Result::fail("Failed to parse JSON: Invalid JSON format");

    return deserialize(parseResult, midiMap);
}

juce::Result MidiMapSerializer::deserialize(const juce::var &jsonVar, MidiMap &midiMap)
{
    if (!jsonVar.isObject())
        return juce::Result::fail("JSON root must be an object");

    auto *object = jsonVar.getDynamicObject();
    if (!object)
        return juce::Result::fail("Failed to get JSON object");

    // Parse groups
    if (object->hasProperty("groups"))
    {
        auto groupsVar = object->getProperty("groups");
        if (groupsVar.isObject())
        {
            if (auto *groupsObject = groupsVar.getDynamicObject())
            {
                for (const auto &property : groupsObject->getProperties())
                {
                    midiMap.groups.push_back({property.name.toString(), property.value.toString()});
                }
            }
        }
    }

    // Parse attributes
    if (object->hasProperty("attributes"))
    {
        auto attributesVar = object->getProperty("attributes");
        if (attributesVar.isObject())
        {
            if (auto *attributesObject = attributesVar.getDynamicObject())
            {
                for (const auto &property : attributesObject->getProperties())
                {
                    midiMap.attributes.push_back({property.name.toString(), property.value.toString()});
                }
            }
        }
    }

    return juce::Result::ok();
}

juce::String MidiMapSerializer::serialize(const MidiMap &midiMap)
{
    auto jsonVar = serializeToVar(midiMap);
    return juce::JSON::toString(jsonVar);
}

juce::var MidiMapSerializer::serializeToVar(const MidiMap &midiMap)
{
    auto *rootObject = new juce::DynamicObject();

    // Add groups
    rootObject->setProperty("groups", createGroupsVar(midiMap.groups));

    // Add attributes
    rootObject->setProperty("attributes", createAttributesVar(midiMap.attributes));

    return juce::var(rootObject);
}

juce::Result MidiMapSerializer::loadFromFile(const juce::File &file, MidiMap &midiMap)
{
    if (!file.exists())
        return juce::Result::fail("File does not exist: " + file.getFullPathName());

    auto jsonString = file.loadFileAsString();
    if (jsonString.isEmpty())
        return juce::Result::fail("File is empty or could not be read: " + file.getFullPathName());

    return deserialize(jsonString, midiMap);
}

juce::Result MidiMapSerializer::saveToFile(const juce::File &file, const MidiMap &midiMap)
{
    auto jsonString = serialize(midiMap);

    // Ensure parent directory exists
    auto parentDir = file.getParentDirectory();
    if (!parentDir.exists())
    {
        auto createResult = parentDir.createDirectory();
        if (!createResult)
            return juce::Result::fail("Failed to create directory: " + parentDir.getFullPathName());
    }

    auto writeResult = file.replaceWithText(jsonString);
    if (!writeResult)
        return juce::Result::fail("Failed to write file: " + file.getFullPathName());

    return juce::Result::ok();
}

//==============================================================================
// Private helper methods

juce::Result MidiMapSerializer::parseGroups(const juce::var &groupsVar, std::vector<std::pair<juce::String, juce::String>> &groups)
{
    if (!groupsVar.isObject())
        return juce::Result::fail("'groups' field must be an object");

    auto *groupsObject = groupsVar.getDynamicObject();
    if (groupsObject == nullptr)
        return juce::Result::fail("Failed to get groups object");

    for (const auto &property : groupsObject->getProperties())
    {
        auto key = property.name.toString();
        auto value = property.value.toString();

        if (key.isEmpty() || value.isEmpty())
            return juce::Result::fail("Group ID and name cannot be empty");

        groups.push_back({key, value});
    }

    return juce::Result::ok();
}

juce::Result MidiMapSerializer::parseAttributes(const juce::var &attributesVar, std::vector<std::pair<juce::String, juce::String>> &attributes)
{
    if (!attributesVar.isObject())
        return juce::Result::fail("'attributes' field must be an object");

    auto *attributesObject = attributesVar.getDynamicObject();
    if (attributesObject == nullptr)
        return juce::Result::fail("Failed to get attributes object");

    for (const auto &property : attributesObject->getProperties())
    {
        auto key = property.name.toString();
        auto value = property.value.toString();

        if (key.isEmpty() || value.isEmpty())
            return juce::Result::fail("Attribute ID and name cannot be empty");

        attributes.push_back({key, value});
    }

    return juce::Result::ok();
}

juce::var MidiMapSerializer::createGroupsVar(const std::vector<std::pair<juce::String, juce::String>> &groups)
{
    auto *groupsObject = new juce::DynamicObject();

    for (const auto &pair : groups)
    {
        groupsObject->setProperty(pair.first, pair.second);
    }

    return juce::var(groupsObject);
}

juce::var MidiMapSerializer::createAttributesVar(const std::vector<std::pair<juce::String, juce::String>> &attributes)
{
    auto *attributesObject = new juce::DynamicObject();

    for (const auto &pair : attributes)
    {
        attributesObject->setProperty(pair.first, pair.second);
    }

    return juce::var(attributesObject);
}
