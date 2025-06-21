#!/bin/bash

# JUCE Plugin Build Script
# Supports multiple JUCE installation methods

set -e

echo "🎵 Kadmium DMX Plugin Builder"
echo "=============================="

# Check if build directory exists
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# Method selection
if [ -f "/usr/local/vcpkg/scripts/buildsystems/vcpkg.cmake" ] && [ -d "/usr/local/vcpkg/installed/x64-linux/share/JUCE" ]; then
    echo "📦 Using vcpkg JUCE installation"
    cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg-cmake-toolchain.cmake -DCMAKE_PREFIX_PATH=/usr/local/vcpkg/installed/x64-linux -DCMAKE_BUILD_TYPE=Debug
elif [ -d "../external/JUCE" ]; then
    echo "🔗 Using git submodule JUCE installation"
    cp ../CMakeLists-Submodule.txt ../CMakeLists.txt
    cmake .. -DCMAKE_BUILD_TYPE=Release
else
    echo "⬇️  Using FetchContent to download JUCE"
    cp ../CMakeLists-FetchContent.txt ../CMakeLists.txt
    cmake .. -DCMAKE_BUILD_TYPE=Release
fi

echo "🛠️  Building plugin..."
cmake --build . --config Release

echo "✅ Build complete!"
echo ""
echo "Plugin files generated:"
find . -name "*.vst3" -o -name "*.au" -o -name "*Standalone*" | head -5
