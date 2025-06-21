# Kadmium DMX DAW Plugin

A JUCE-based audio plugin for DMX lighting control in digital audio workstations.

## Getting Started

### Prerequisites
- C++17 or later compiler (GCC, Clang, or MSVC)
- CMake 3.22+
- Git

### Build Options

#### Option 1: Using vcpkg (Recommended)
```bash
# Install JUCE via vcpkg
vcpkg install juce

# Configure with CMake
mkdir build
cd build
cmake .. -DCMAKE_TOOLCHAIN_FILE=/usr/local/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build .
```

#### Option 2: Using Git Submodules
```bash
# Clone JUCE as submodule
git submodule add https://github.com/juce-framework/JUCE.git external/JUCE
git submodule update --init --recursive

# Build
mkdir build
cd build
cmake ..
cmake --build .
```

#### Option 3: Using FetchContent (CMake)
CMake will automatically download JUCE during configuration.

## Building

```bash
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build . --config Release
```

## Plugin Formats
- VST3
- AU (macOS)
- Standalone Application

## Project Structure
```
Source/
├── PluginProcessor.cpp    # Main audio processing
├── PluginEditor.cpp       # GUI components
└── PluginProcessor.h      # Header files
```