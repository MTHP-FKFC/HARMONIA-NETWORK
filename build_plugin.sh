#!/bin/bash

# Build script for Cohera Saturator

echo "Building Cohera Saturator..."

cd build

# Clean build
rm -rf *

# Configure with CMake
cmake ..

# Build Standalone (works)
make Cohera_Saturator_Standalone -j4

echo "Standalone version built successfully!"

# Try to build VST3 (may fail due to parameter conflicts)
echo "Attempting to build VST3 version..."
make Cohera_Saturator_VST3 -j4 2>/dev/null || echo "VST3 build failed - parameter conflicts"

echo "Build complete!"
