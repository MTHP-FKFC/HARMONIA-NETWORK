#!/bin/bash

# Cohera Saturator - Release Build Script
# Builds only the plugin artifacts (VST3, AU, Standalone) without tests or debug info.

echo "🚀 Starting Release Build..."

# 1. Clean build directory
if [ -d "build_release" ]; then
    echo "🧹 Cleaning previous release build..."
    rm -rf build_release
fi

mkdir build_release
cd build_release

# 2. Configure CMake
# - CMAKE_BUILD_TYPE=Release: Optimizations enabled (-O3), debug info disabled
# - BUILD_TESTS=OFF: Skip building test executables
echo "⚙️  Configuring CMake (Release Mode)..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Release \
    -DBUILD_TESTS=OFF \
    -G "Unix Makefiles"

if [ $? -ne 0 ]; then
    echo "❌ CMake configuration failed!"
    exit 1
fi

# 3. Build Plugin Targets
echo "🔨 Building Plugin Targets..."
cmake --build . --target Cohera_Saturator -j8

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo "✅ Release Build Complete!"
echo "📦 Artifacts location:"
echo "   VST3:       build_release/Cohera_Saturator_artefacts/VST3/"
echo "   AU:         build_release/Cohera_Saturator_artefacts/AU/"
echo "   Standalone: build_release/Cohera_Saturator_artefacts/Standalone/"
