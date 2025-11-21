# 🎸 COHERA SATURATOR - ABLETON TROUBLESHOOTING GUIDE

## ✅ ПРОВЕРЬ СНАЧАЛА

### 1. Плагин установлен правильно?
```bash
ls -lh ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3
ls -lh ~/Library/Audio/Plug-Ins/Components/Cohera\ Saturator.component
```

Должны быть файлы ~10MB

### 2. Плагин валидный?
```bash
pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3
```

Должен показать "SUCCESS"

### 3. Код подписан?
```bash
codesign -dv ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3 2>&1 | grep "Signature"
```

Должно быть "Signature=adhoc"

---

## 🔧 РЕШЕНИЯ ПО ПОРЯДКУ

### Решение 1: Стандартный Fix (НАЧНИ С ЭТОГО)
```bash
cd ~/Documents/JUCE/Cohera_Saturator
./fix_ableton.sh
```

Затем:
1. Открой Ableton
2. Preferences > Plug-ins
3. Rescan
4. Ищи "Cohera Saturator" в Browser > Plug-ins > Audio Effects

---

### Решение 2: Deep Clean (если Решение 1 не помогло)
```bash
cd ~/Documents/JUCE/Cohera_Saturator
./deep_clean_ableton.sh
```

Затем перезагрузи Mac и открой Ableton

---

### Решение 3: Проверь настройки Ableton

**В Ableton Live 12:**
1. Preferences > Plug-ins
2. Убедись, что `Use VST3 Plug-ins` = ON
3. Проверь `VST3 Custom Folder` (должна быть пустой если используешь системную папку)
4. Нажми `Rescan`

**Важно:**
- Не добавляй `~/Library/Audio/Plug-Ins/VST3` в Custom Folder
- Ableton сканирует системную папку автоматически

---

### Решение 4: Manual Plugin Database Reset

```bash
# 1. Закрой Ableton
killall "Ableton Live 12"

# 2. Удали базу данных (Ableton пересоздаст её)
rm -rf ~/Library/Preferences/Ableton/Live\ 12.2/Database

# 3. Удали кэш Library.cfg
rm ~/Library/Preferences/Ableton/Live\ 12.2/Library.cfg

# 4. Открой Ableton (подожди 2-3 минуты на первое сканирование)
```

---

### Решение 5: Проверь логи Ableton

```bash
# Открой лог-файл
cat ~/Library/Preferences/Ableton/Live\ 12.2/Log.txt | grep -i "cohera\|vst3"
```

**Ищи:**
- "Failed to load" → проблема с подписью
- "Blacklisted" → плагин в чёрном списке
- "Scan failed" → проблема при сканировании

**Если в чёрном списке:**
```bash
# Удали черный список
rm ~/Library/Preferences/Ableton/Live\ 12.2/PluginBlackList.cfg
```

---

### Решение 6: Nuclear Option (последний шанс)

⚠️ **ЭТО СБРОСИТ ВСЕ НАСТРОЙКИ ABLETON!**

```bash
# 1. Сделай бэкап
cp -r ~/Library/Preferences/Ableton ~/Desktop/Ableton_Backup

# 2. Полный сброс
killall "Ableton Live 12"
rm ~/Library/Preferences/com.ableton.live.plist
rm -rf ~/Library/Preferences/Ableton

# 3. Открой Ableton (он пересоздаст всё с нуля)
```

---

## 🧪 ТЕСТ В ДРУГИХ DAW (для диагностики)

### Logic Pro (тестирует AU)
```bash
# Logic автоматически сканирует ~/Library/Audio/Plug-Ins/Components
# Просто открой Logic > Audio FX > Audio Units > CoheraAudio > Cohera Saturator
```

### Reaper (тестирует VST3)
```bash
# Reaper > Preferences > VST > Re-scan
# Должен найти Cohera Saturator
```

Если плагин работает в Logic/Reaper но НЕ в Ableton → проблема специфична для Ableton

---

## 🔍 ДИАГНОСТИКА

### Проверь версию Ableton
```bash
defaults read ~/Library/Preferences/com.ableton.live.plist CFBundleShortVersionString
```

Cohera Saturator тестировался на Ableton Live 12.0+

### Проверь путь сканирования
```bash
defaults read ~/Library/Preferences/com.ableton.live.plist VST3CustomFolderPath
```

Должно быть пусто или не существовать (используется системная папка)

---

## 📝 ЧЕКЛИСТ ПЕРЕД ОБРАЩЕНИЕМ ЗА ПОМОЩЬЮ

Убедись что сделал:
- [ ] `./fix_ableton.sh` выполнен успешно
- [ ] pluginval показывает SUCCESS
- [ ] Перезапустил Ableton после установки
- [ ] Rescan в Ableton Preferences
- [ ] Проверил логи Ableton
- [ ] Тестировал в другой DAW (Logic/Reaper)
- [ ] Пробовал deep_clean_ableton.sh

**Если всё перепробовал и не помогло:**

Создай issue с этой информацией:
```bash
# Запусти это и пришли вывод:
echo "=== System Info ==="
sw_vers
echo "=== Ableton Version ==="
defaults read ~/Library/Preferences/com.ableton.live.plist CFBundleShortVersionString
echo "=== Plugin Info ==="
ls -lh ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3
codesign -dv ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3 2>&1
echo "=== Plugin Validation ==="
pluginval --strictness-level 5 ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3 2>&1 | tail -20
echo "=== Ableton Logs ==="
tail -100 ~/Library/Preferences/Ableton/Live\ 12.2/Log.txt | grep -i "cohera\|vst"
```

---

## 🎯 ИЗВЕСТНЫЕ ПРОБЛЕМЫ

### macOS Gatekeeper
Если видишь предупреждение "cannot be opened because the developer cannot be verified":
```bash
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3
```

### Apple Silicon (M1/M2/M3)
Плагин собран для ARM64. Если используешь Intel Mac или Rosetta:
```bash
# Проверь архитектуру
lipo -info ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3/Contents/MacOS/Cohera\ Saturator
```

Должно показать "arm64"

---

## 🚀 ЕСЛИ ВСЁ РАБОТАЕТ

После успешной установки можешь запустить:
```bash
cd ~/Documents/JUCE/Cohera_Saturator
./install_release_plugins.sh
```

Это установит плагины из релизной сборки (более оптимизированные).

**Happy producing! 🎸**
