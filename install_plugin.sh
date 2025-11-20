#!/bin/bash

# Installation script for Cohera Saturator plugins

echo "Installing Cohera Saturator plugins..."

# VST3 installation directory for macOS
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"

# AU installation directory for macOS
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"

# Create directories if they don't exist
mkdir -p "$VST3_DIR"
mkdir -p "$AU_DIR"

# Install VST3 plugin
if [ -d "build/Cohera_Saturator_artefacts/VST3/Cohera Saturator.vst3" ]; then
    echo "Installing VST3 plugin..."
    cp -r "build/Cohera_Saturator_artefacts/VST3/Cohera Saturator.vst3" "$VST3_DIR/"
    echo "✅ VST3 plugin installed to: $VST3_DIR"
else
    echo "❌ VST3 plugin not found. Build it first."
fi

# Install AU plugin (if exists)
if [ -d "build/Cohera_Saturator_artefacts/AU/Cohera Saturator.component" ]; then
    echo "Installing AU plugin..."
    cp -r "build/Cohera_Saturator_artefacts/AU/Cohera Saturator.component" "$AU_DIR/"
    echo "✅ AU plugin installed to: $AU_DIR"
else
    echo "ℹ️  AU plugin not built (add AU to FORMATS in CMakeLists.txt to build it)"
fi

# Standalone is always available
if [ -d "build/Cohera_Saturator_artefacts/Standalone/Cohera Saturator.app" ]; then
    echo "ℹ️  Standalone app available at: build/Cohera_Saturator_artefacts/Standalone/Cohera Saturator.app"
fi

echo ""
echo "🎸 Installation complete! Your Cohera Saturator is ready to rock! 🎸"
echo ""
echo "Test it in your favorite DAW!"
