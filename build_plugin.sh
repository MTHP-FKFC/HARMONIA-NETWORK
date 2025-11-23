#!/bin/bash

# Build script for Cohera Saturator (Release Mode)

echo "🔨 Building Cohera Saturator in RELEASE mode..."

cd build

# Clean build directory
echo "🧹 Cleaning build directory..."
rm -rf *

# Configure with CMake in Release mode
echo "⚙️  Configuring CMake for Release build..."
cmake -DCMAKE_BUILD_TYPE=Release -DJUCE_DIR="/Users/macos/JUCE" ..

# Get number of CPU cores for parallel build
CPU_CORES=$(sysctl -n hw.ncpu 2>/dev/null || nproc 2>/dev/null || echo 4)
echo "🚀 Building with $CPU_CORES CPU cores..."

# Build all plugin formats at once
echo "🎛️  Building all plugin formats..."
make HARMONIA_NETWORK_All -j$CPU_CORES

if [ $? -eq 0 ]; then
    echo ""
    echo "✅ ALL PLUGINS BUILT SUCCESSFULLY!"
    echo "📦 Available formats:"
    echo "   • Standalone: build/HARMONIA_NETWORK_artefacts/Standalone/"
    echo "   • AU: build/HARMONIA_NETWORK_artefacts/AU/"
    echo "   • VST3: build/HARMONIA_NETWORK_artefacts/VST3/"
    echo ""
    echo "🚀 Run './install_release_plugins.sh' to install plugins system-wide"
else
    echo "❌ Build failed!"
    exit 1
fi
