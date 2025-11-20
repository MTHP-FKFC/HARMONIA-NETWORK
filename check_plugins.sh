#!/bin/bash

echo "🔍 Checking Cohera Saturator plugins..."
echo ""

# Check AU plugin
echo "🎹 AU Plugin (macOS native):"
if [ -d ~/Library/Audio/Plug-Ins/Components/Cohera\ Saturator.component ]; then
    echo "✅ Installed: ~/Library/Audio/Plug-Ins/Components/Cohera Saturator.component"
    ls -la ~/Library/Audio/Plug-Ins/Components/Cohera\ Saturator.component/Contents/MacOS/
else
    echo "❌ Not found"
fi
echo ""

# Check VST3 plugin  
echo "🎛️  VST3 Plugin:"
if [ -d ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3 ]; then
    echo "✅ Installed: ~/Library/Audio/Plug-Ins/VST3/Cohera Saturator.vst3"
    ls -la ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3/Contents/MacOS/
else
    echo "❌ Not found"
fi
echo ""

echo "💡 Tips for Ableton Live:"
echo "1. Restart Ableton Live completely"
echo "2. Go to Preferences > Audio > Rescan Plug-ins"
echo "3. Check if plugins appear in Audio Effects"
echo "4. Try placing plugins in system folders if user folders don't work"
echo ""

echo "🎸 Ready to rock!"
