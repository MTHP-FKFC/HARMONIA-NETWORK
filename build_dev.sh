#!/bin/bash

# Cohera Saturator - Development Build Script
# Builds plugin artifacts AND test suites. Debug symbols enabled.

echo "🛠️  Starting Development Build..."

# 1. Create build directory if needed
if [ ! -d "build" ]; then
    mkdir build
fi
cd build

# 2. Configure CMake
# - CMAKE_BUILD_TYPE=Debug: Debug symbols enabled, optimizations disabled (usually)
# - BUILD_TESTS=ON: Build test executables
echo "⚙️  Configuring CMake (Debug Mode)..."
cmake .. \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_TESTS=ON \
    -G "Unix Makefiles"

if [ $? -ne 0 ]; then
    echo "❌ CMake configuration failed!"
    exit 1
fi

# 3. Build All Targets
echo "🔨 Building All Targets..."
cmake --build . --target all -j8

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo "✅ Development Build Complete!"
echo "📦 Artifacts location:"
echo "   VST3:  build/Cohera_Saturator_artefacts/VST3/"
echo "   Tests: build/tests/"
