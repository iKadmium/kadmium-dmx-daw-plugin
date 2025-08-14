# Parameter System Documentation

## Overview

The Kadmium DMX Plugin now includes a robust parameter management system built on top of JUCE's `AudioProcessorValueTreeState` (APVTS). This system provides both static parameter definitions and a dynamic interface for parameter access.

## Current Parameters

- **Hue**: 0-360 degrees (default: 0)
- **Saturation**: 0-100% (default: 100%)
- **Brightness**: 0-100% (default: 100%)

## Architecture

### Static Parameter Definition
Parameters are defined statically in `createParameterLayout()` and registered with the APVTS system. This ensures proper host automation, preset saving/loading, and threading safety.

### Dynamic Parameter Access
The system provides several utility functions for dynamic parameter management:

```cpp
// Get parameter value by ID
float getParameterValue(const juce::String& parameterID) const;

// Set parameter value by ID
void setParameterValue(const juce::String& parameterID, float value);

// Get all parameter IDs for iteration
juce::StringArray getAllParameterIDs() const;

// Get parameter metadata for UI generation
ParameterInfo getParameterInfo(const juce::String& parameterID) const;
```

### ParameterInfo Structure
The `ParameterInfo` struct provides metadata for dynamic UI generation:

```cpp
struct ParameterInfo {
    juce::String id;          // Parameter ID
    juce::String name;        // Display name
    float minValue;           // Minimum value
    float maxValue;           // Maximum value
    float defaultValue;       // Default value
    juce::String unit;        // Unit string (e.g., "%", "degrees")
};
```

## Usage Examples

### Accessing Parameters in processBlock()
```cpp
void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages) {
    float hue = getParameterValue(HUE_PARAM_ID);
    float saturation = getParameterValue(SATURATION_PARAM_ID);
    float brightness = getParameterValue(BRIGHTNESS_PARAM_ID);
    
    // Process HSB values for DMX output...
}
```

### Dynamic UI Generation
```cpp
void createParameterControls() {
    auto parameterIDs = getAllParameterIDs();
    
    for (const auto& paramID : parameterIDs) {
        auto info = getParameterInfo(paramID);
        
        // Create slider with proper range
        auto* slider = new juce::Slider();
        slider->setRange(info.minValue, info.maxValue);
        slider->setValue(getParameterValue(paramID));
        
        // Create label
        auto* label = new juce::Label();
        label->setText(info.name + " (" + info.unit + ")", juce::dontSendNotification);
    }
}
```

### Programmatic Parameter Control
```cpp
// Set parameters programmatically
setParameterValue(HUE_PARAM_ID, 180.0f);      // Cyan
setParameterValue(SATURATION_PARAM_ID, 75.0f); // 75% saturation
setParameterValue(BRIGHTNESS_PARAM_ID, 90.0f); // 90% brightness
```

## Future Extensions

This system can be extended to support:

1. **Runtime Parameter Addition**: While JUCE parameters must be defined at construction time, you could create a pool of "generic" parameters and map them dynamically to different functions.

2. **Parameter Groups**: Organize parameters into logical groups (e.g., "Color", "Effects", "DMX Channels").

3. **Complex Parameter Types**: Support for choice parameters, boolean parameters, etc.

4. **MIDI Learn**: Map MIDI controllers to parameters dynamically.

5. **Automation Curves**: Advanced automation and modulation systems.

## Benefits

- **Thread Safety**: Built on JUCE's proven APVTS system
- **Host Integration**: Automatic support for automation, presets, and undo/redo
- **Type Safety**: Compile-time parameter ID validation
- **Extensibility**: Easy to add new parameters without breaking existing code
- **Dynamic Access**: Runtime iteration and manipulation of parameters
