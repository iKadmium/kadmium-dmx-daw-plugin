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
echo "📦 Using vcpkg JUCE installation"
cmake .. -DCMAKE_TOOLCHAIN_FILE=../vcpkg-cmake-toolchain.cmake -DCMAKE_PREFIX_PATH=/usr/local/vcpkg/installed/x64-linux -DCMAKE_BUILD_TYPE=Debug

echo "🛠️  Building plugin..."
cmake --build . --config Release

echo "✅ Build complete!"
echo ""
echo "Plugin files generated:"
find . -name "*.vst3" -o -name "*.au" -o -name "*Standalone*" | head -5
