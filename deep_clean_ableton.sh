#!/bin/bash

# Deep Clean Ableton Plugin Cache
# Используй это, если обычный fix_ableton.sh не помог

echo "🧹 DEEP CLEAN ABLETON CACHE"
echo "==========================="
echo ""

# Останавливаем Ableton
echo "Stopping Ableton Live..."
killall "Ableton Live 12" 2>/dev/null || killall "Ableton Live 11" 2>/dev/null || killall "Live" 2>/dev/null || true
sleep 3

# Убиваем все связанные процессы
echo "Killing audio services..."
killall -9 AudioComponentRegistrar 2>/dev/null || true
killall -9 coreaudiod 2>/dev/null || true
sleep 2

# Удаляем базу данных плагинов Ableton
echo "Removing Ableton plugin database..."
rm -rf ~/Library/Preferences/Ableton/Live\ */Database 2>/dev/null || true
echo "  ✅ Removed plugin database"

# Удаляем все .cfg кэши
echo "Removing .cfg cache files..."
rm -rf ~/Library/Preferences/Ableton/Live\ */Cache/*.cfg 2>/dev/null || true
rm -f ~/Library/Preferences/Ableton/Live\ */Library.cfg 2>/dev/null || true
echo "  ✅ Removed .cfg files"

# Удаляем системные AU кэши
echo "Removing AU caches..."
rm -rf ~/Library/Caches/AudioUnitCache 2>/dev/null || true
rm -rf /Library/Caches/com.apple.audiounits.cache 2>/dev/null || true
sudo rm -rf /Library/Caches/com.apple.audio.InfoCache.plist 2>/dev/null || true
echo "  ✅ Removed AU caches"

# Перезапускаем coreaudiod (это безопасно)
echo "Restarting Core Audio..."
sudo kill all -9 coreaudiod 2>/dev/null || true
sleep 2
echo "  ✅ Core Audio restarted"

echo ""
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo "✅ DEEP CLEAN COMPLETE!"
echo "━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━"
echo ""
echo "📋 NEXT: Open Ableton and rescan plugins"
echo ""
echo "If plugin STILL doesn't appear, try this nuclear option:"
echo ""
echo "  rm ~/Library/Preferences/com.ableton.live.plist"
echo "  rm -rf ~/Library/Preferences/Ableton"
echo ""
echo "⚠️  This will reset ALL Ableton preferences!"
