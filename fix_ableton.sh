#!/bin/bash

# Cohera Saturator - Ableton Fix Script
# Исправляет проблемы с загрузкой плагина в Ableton Live

set -e

echo "🔧 COHERA SATURATOR - ABLETON FIX"
echo "=================================="
echo ""

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Paths
VST3_PATH=~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3
AU_PATH=~/Library/Audio/Plug-Ins/Components/Cohera\ Saturator.component
BUILD_VST3=build/Cohera_Saturator_artefacts/Release/VST3/Cohera\ Saturator.vst3
BUILD_AU=build/Cohera_Saturator_artefacts/Release/AU/Cohera\ Saturator.component

echo "Step 1: Stopping Ableton Live..."
killall "Ableton Live 12" 2>/dev/null || killall "Ableton Live 11" 2>/dev/null || killall "Live" 2>/dev/null || echo "  ℹ️  Ableton not running"
sleep 2

echo ""
echo "Step 2: Removing old plugins..."
if [ -e "$VST3_PATH" ]; then
    rm -rf "$VST3_PATH"
    echo "  ✅ Removed old VST3"
else
    echo "  ℹ️  No old VST3 found"
fi

if [ -e "$AU_PATH" ]; then
    rm -rf "$AU_PATH"
    echo "  ✅ Removed old AU"
else
    echo "  ℹ️  No old AU found"
fi

echo ""
echo "Step 3: Clearing plugin caches..."
# Ableton cache
ABLETON_CACHE=~/Library/Preferences/Ableton
if [ -d "$ABLETON_CACHE" ]; then
    rm -rf "$ABLETON_CACHE/Live "*/Cache/*.cfg 2>/dev/null || true
    echo "  ✅ Cleared Ableton cache"
fi

# macOS audio unit cache
AUDIO_CACHE=~/Library/Caches/AudioUnitCache
if [ -d "$AUDIO_CACHE" ]; then
    rm -rf "$AUDIO_CACHE"
    echo "  ✅ Cleared AU cache"
fi

# Kill audio component service
killall -9 AudioComponentRegistrar 2>/dev/null || true
sleep 1

echo ""
echo "Step 4: Installing fresh plugins..."
if [ -e "$BUILD_VST3" ]; then
    cp -R "$BUILD_VST3" ~/Library/Audio/Plug-Ins/VST3/
    echo "  ✅ Installed VST3"
    
    # Sign the plugin
    codesign --force --deep --sign - "$VST3_PATH" 2>/dev/null
    echo "  ✅ Signed VST3"
    
    # Remove quarantine
    xattr -cr "$VST3_PATH" 2>/dev/null
    echo "  ✅ Removed quarantine flags"
else
    echo -e "  ${RED}❌ VST3 build not found! Run ./build_plugin.sh first${NC}"
    exit 1
fi

if [ -e "$BUILD_AU" ]; then
    cp -R "$BUILD_AU" ~/Library/Audio/Plug-Ins/Components/
    echo "  ✅ Installed AU"
    
    codesign --force --deep --sign - "$AU_PATH" 2>/dev/null
    echo "  ✅ Signed AU"
    
    xattr -cr "$AU_PATH" 2>/dev/null
    echo "  ✅ Removed quarantine flags"
else
    echo -e "  ${YELLOW}⚠️  AU build not found (optional)${NC}"
fi

echo ""
echo "Step 5: Validating VST3..."
if command -v pluginval &> /dev/null; then
    echo "  Running pluginval (this takes ~30 seconds)..."
    pluginval --strictness-level 5 --validate-in-process --output-dir /tmp "$VST3_PATH" > /tmp/pluginval.log 2>&1
    
    if grep -q "SUCCESS" /tmp/pluginval.log; then
        echo -e "  ${GREEN}✅ Plugin validation PASSED${NC}"
    else
        echo -e "  ${RED}❌ Plugin validation FAILED${NC}"
        echo "  Check /tmp/pluginval.log for details"
    fi
else
    echo "  ℹ️  pluginval not installed, skipping validation"
    echo "  Install with: brew install pluginval"
fi

echo ""
echo "Step 6: Verifying installation..."
if [ -e "$VST3_PATH" ]; then
    VST3_SIZE=$(du -sh "$VST3_PATH" | awk '{print $1}')
    echo "  ✅ VST3 installed: $VST3_SIZE"
    
    # Check signature
    SIGNATURE=$(codesign -dv "$VST3_PATH" 2>&1 | grep "Signature=" | awk -F= '{print $2}')
    echo "  ✅ Code signature: $SIGNATURE"
else
    echo -e "  ${RED}❌ VST3 installation failed${NC}"
    exit 1
fi

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo -e "${GREEN}✅ INSTALLATION COMPLETE!${NC}"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "📋 NEXT STEPS FOR ABLETON LIVE:"
echo ""
echo "1. Open Ableton Live"
echo "2. Go to: Preferences > Plug-ins"
echo "3. Click 'Rescan' next to 'VST3 Custom Folder'"
echo "4. Wait for scan to complete (~30 seconds)"
echo "5. Close and reopen Preferences"
echo "6. Look for 'Cohera Saturator' in Browser > Plug-ins > Audio Effects"
echo ""
echo "🔍 TROUBLESHOOTING:"
echo ""
echo "If plugin still doesn't appear:"
echo ""
echo "  Option A: Manual rescan"
echo "    1. Preferences > Plug-ins"
echo "    2. Turn OFF 'Use VST3 Plug-ins'"
echo "    3. Wait 5 seconds"
echo "    4. Turn ON 'Use VST3 Plug-ins'"
echo "    5. Click 'Rescan'"
echo ""
echo "  Option B: Reset Ableton database"
echo "    1. Quit Ableton"
echo "    2. Run: rm -rf ~/Library/Preferences/Ableton/Live\\ */Database"
echo "    3. Restart Ableton (will rebuild database)"
echo ""
echo "  Option C: Check Ableton logs"
echo "    ~/Library/Preferences/Ableton/Live */Log.txt"
echo "    Look for 'Cohera' or 'VST3' errors"
echo ""
echo "  Option D: Test in other DAWs"
echo "    - Logic Pro (uses AU)"
echo "    - Reaper (uses VST3)"
echo "    - FL Studio (uses VST3)"
echo ""
echo "🎸 Happy producing!"
