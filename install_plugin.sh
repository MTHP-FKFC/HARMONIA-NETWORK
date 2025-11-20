#!/bin/bash

# Installation script for Cohera Saturator VST3 plugin

echo "Installing Cohera Saturator..."

# VST3 installation directory for macOS
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"

# Create directory if it doesn't exist
mkdir -p "$VST3_DIR"

# Copy VST3 plugin if it exists
if [ -d "build/Cohera_Saturator_artefacts/VST3/Cohera Saturator.vst3" ]; then
    cp -r "build/Cohera_Saturator_artefacts/VST3/Cohera Saturator.vst3" "$VST3_DIR/"
    echo "VST3 plugin installed to: $VST3_DIR"
else
    echo "VST3 plugin not found. Build it first with: make Cohera_Saturator_VST3"
    echo "Standalone version is available at: build/Cohera_Saturator_artefacts/Standalone/Cohera Saturator.app"
fi

echo "Installation complete!"
