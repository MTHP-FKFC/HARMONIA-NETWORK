#!/bin/bash

echo "🔥 COHERA SATURATOR RELEASE INSTALLER 🔥"
echo "========================================"

# Пути к плагинам
AU_PLUGIN="build/Cohera_Saturator_artefacts/Release/AU/Cohera Saturator.component"
VST3_PLUGIN="build/Cohera_Saturator_artefacts/Release/VST3/Cohera Saturator.vst3"

# Системные директории
AU_DIR="$HOME/Library/Audio/Plug-Ins/Components"
VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"

# Создаем директории если не существуют
mkdir -p "$AU_DIR"
mkdir -p "$VST3_DIR"

echo "📦 Installing AU Plugin..."
if [ -d "$AU_PLUGIN" ]; then
    # Remove old version if exists
    if [ -d "$AU_DIR/Cohera Saturator.component" ]; then
        rm -rf "$AU_DIR/Cohera Saturator.component"
    fi
    cp -r "$AU_PLUGIN" "$AU_DIR/"
    echo "✅ AU Plugin installed to: $AU_DIR"
else
    echo "❌ AU Plugin not found: $AU_PLUGIN"
fi

echo "📦 Installing VST3 Plugin..."
if [ -d "$VST3_PLUGIN" ]; then
    # Remove old version if exists
    if [ -d "$VST3_DIR/Cohera Saturator.vst3" ]; then
        rm -rf "$VST3_DIR/Cohera Saturator.vst3"
    fi
    cp -r "$VST3_PLUGIN" "$VST3_DIR/"
    echo "✅ VST3 Plugin installed to: $VST3_DIR"
else
    echo "❌ VST3 Plugin not found: $VST3_PLUGIN"
fi

echo ""
echo "🎯 INSTALLATION COMPLETE!"
echo "Restart your DAW (Logic Pro, Ableton, etc.) to load the plugins."
echo ""
echo "Plugin Info:"
echo "- Name: Cohera Saturator"
echo "- Company: CoheraAudio"
echo "- Formats: AU, VST3"
echo "- Version: 0.1.0"
