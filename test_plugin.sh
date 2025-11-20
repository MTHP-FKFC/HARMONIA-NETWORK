#!/bin/bash

echo "🧪 Testing Cohera Saturator plugin..."
echo ""

# Test Standalone app
echo "📱 Testing Standalone app..."
if [ -f "build/Cohera_Saturator_artefacts/Standalone/Cohera Saturator.app/Contents/MacOS/Cohera Saturator" ]; then
    echo "✅ Standalone executable exists"
    ls -lh "build/Cohera_Saturator_artefacts/Standalone/Cohera Saturator.app/Contents/MacOS/Cohera Saturator"
else
    echo "❌ Standalone executable not found"
fi
echo ""

# Test AU plugin
echo "🎹 Testing AU plugin..."
if [ -f "/Users/$USER/Library/Audio/Plug-Ins/Components/Cohera Saturator.component/Contents/MacOS/Cohera Saturator" ]; then
    echo "✅ AU plugin installed in user folder"
    ls -lh "/Users/$USER/Library/Audio/Plug-Ins/Components/Cohera Saturator.component/Contents/MacOS/Cohera Saturator"
else
    echo "❌ AU plugin not found in user folder"
fi
echo ""

# Test VST3 plugin
echo "🎛️  Testing VST3 plugin..."
if [ -f "/Users/$USER/Library/Audio/Plug-Ins/VST3/Cohera Saturator.vst3/Contents/MacOS/Cohera Saturator" ]; then
    echo "✅ VST3 plugin installed"
    ls -lh "/Users/$USER/Library/Audio/Plug-Ins/VST3/Cohera Saturator.vst3/Contents/MacOS/Cohera Saturator"
else
    echo "❌ VST3 plugin not found"
fi
echo ""

echo "🎯 Next steps for Ableton Live:"
echo "1. Completely quit Ableton Live"
echo "2. Run: killall -9 Ableton Live 2>/dev/null || true"
echo "3. Wait 10 seconds"
echo "4. Restart Ableton Live"
echo "5. Go to Preferences > Plug-ins > Rescan"
echo "6. Look for 'Cohera Saturator' in Audio Effects"
echo ""

echo "🔧 Alternative: Test with other DAWs:"
echo "- Logic Pro"
echo "- Reaper"  
echo "- FL Studio"
echo "- Pro Tools"
echo ""

echo "🚀 Standalone app should always work!"
