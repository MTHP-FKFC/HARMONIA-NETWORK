# Руководство: Почему плагины не регистрируются и как это исправить

## Краткое резюме
Проблемы с загрузкой VST3 и AU плагинов в macOS возникают из-за:
1. **Отсутствия подписи кода** (code signature)
2. **Неправильных 4-символьных кодов** в AU (автогенерация JUCE)
3. **Quarantine флагов** от macOS Gatekeeper
4. **Кэширования** в DAW и системных службах

---

## Часть 1: VST3 — Проблема с подписью кода

### Почему это происходит?
Начиная с macOS Catalina (10.15), Apple требует **code signature** для всех исполняемых бинарников. Даже для разработки нужна как минимум **ad-hoc подпись** (`codesign --sign -`).

**Симптомы:**
```bash
# Плагин не появляется в Ableton Live
# В консоли (Console.app) можно увидеть:
codesign: validation failed
```

### Как это лечить?
```bash
# 1. Подписать VST3 после сборки:
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3

# 2. Убрать quarantine флаг (если качали плагин из интернета):
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3

# 3. Проверить подпись:
codesign -dv ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3
# Должно быть: Signature=adhoc
```

### Как избегать?
Добавьте автоподпись в **build_plugin.sh**:
```bash
#!/bin/bash
cd build && cmake .. && make Cohera_Saturator_VST3 -j4

# Автоматическая подпись после сборки:
VST3_PATH="$PWD/Cohera_Saturator_artefacts/Release/VST3/Cohera Saturator.vst3"
if [ -d "$VST3_PATH" ]; then
    codesign --force --deep --sign - "$VST3_PATH"
    echo "✅ VST3 signed"
fi
```

---

## Часть 2: AU (Audio Unit) — Проблема с 4-символьными кодами

### Почему это происходит?
macOS регистрирует AU плагины по **4-символьным кодам** (FourCharCode):
- `manufacturer` — кто сделал (например, `"Cohr"` для CoheraAudio)
- `subtype` — какой это плагин (например, `"Csat"` для Cohera Saturator)
- `type` — тип эффекта (`"aufx"` = Audio Effect)

JUCE **автоматически генерирует** коды из хэша имени плагина, если вы их не укажете. Это приводит к **случайным кодам** типа `"Manu"` и `"Qztq"`, которые:
1. Не совпадают с ожидаемыми при валидации (`auval -v aufx Csat Cohr`)
2. Могут конфликтовать с другими плагинами
3. Меняются при переименовании проекта

**Симптомы:**
```bash
$ auval -v aufx Csat Cohr
# ERROR: Cannot get Component's Name strings
# FATAL ERROR: didn't find the component

$ plutil -p "Info.plist" | grep -A5 AudioComponents
# "manufacturer" => "Manu"  ❌ НЕПРАВИЛЬНО
# "subtype" => "Qztq"       ❌ НЕПРАВИЛЬНО
```

### Как это лечить?
**Откройте `CMakeLists.txt`** и явно укажите коды в `juce_add_plugin`:

```cmake
juce_add_plugin(Cohera_Saturator
    COMPANY_NAME "CoheraAudio"
    PLUGIN_MANUFACTURER_CODE Cohr    # ✅ 4 символа
    PLUGIN_CODE Csat                 # ✅ 4 символа, уникальный идентификатор
    AU_MAIN_TYPE kAudioUnitType_Effect  # ✅ 'aufx' для эффектов
    
    # ... остальные параметры
    FORMATS AU VST3 Standalone
)
```

**Затем пересоберите:**
```bash
cd build && cmake .. && make Cohera_Saturator_AU -j4
```

**Проверьте Info.plist:**
```bash
plutil -p "build/.../AU/Cohera Saturator.component/Contents/Info.plist" | grep -A8 AudioComponents

# Должно быть:
# "manufacturer" => "Cohr"  ✅
# "subtype" => "Csat"       ✅
# "type" => "aufx"          ✅
```

### Как избегать?
**Всегда указывайте коды явно** при создании нового JUCE проекта. Правила:
- **Manufacturer Code:** Регистрируйте у Apple (developer.apple.com) или используйте 4 буквы названия компании
- **Plugin Code:** 4 уникальных символа, не должны конфликтовать с другими вашими плагинами
- Документируйте коды в `README.md` или комментариях `CMakeLists.txt`

---

## Часть 3: Кэширование и системные службы

### Почему это происходит?
macOS и DAW кэшируют:
1. **AudioComponentRegistrar** — кэш AU плагинов в памяти
2. **coreaudiod** — Core Audio демон
3. **Ableton Live** — собственный кэш плагинов (`Database/`, `*.cfg`)

Если обновить бинарник, но не очистить кэш, система продолжит использовать старую версию.

### Как это лечить?
**Полная очистка (deep clean):**
```bash
#!/bin/bash
# Убить процессы
killall -9 "Ableton Live 11 Suite" AudioComponentRegistrar coreaudiod 2>/dev/null

# Очистить Ableton
rm -rf ~/Library/Preferences/Ableton/Live\ 11.*/Database/
rm -f ~/Library/Preferences/Ableton/Live\ 11.*/*.cfg

# Очистить AU кэш macOS
rm -rf ~/Library/Caches/AudioUnitCache/
rm -rf /Library/Caches/com.apple.audiounits.cache

# Перезапустить Core Audio
sudo launchctl kickstart -k system/com.apple.audio.coreaudiod

# Подождать 2 секунды перед валидацией
sleep 2
```

**Валидация после установки:**
```bash
# VST3
pluginval --strictness-level 5 --validate ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3

# AU
auval -v aufx Csat Cohr
```

### Как избегать?
Используйте **автоматизированный скрипт** `fix_ableton.sh`:
```bash
#!/bin/bash
# Убить Ableton
killall -9 "Ableton Live 11 Suite" 2>/dev/null

# Очистить кэш
rm -rf ~/Library/Preferences/Ableton/Live\ 11.*/Database/
rm -f ~/Library/Preferences/Ableton/Live\ 11.*/*.cfg

# Установить VST3 + AU
cp -R "build/.../VST3/Cohera Saturator.vst3" ~/Library/Audio/Plug-Ins/VST3/
cp -R "build/.../AU/Cohera Saturator.component" ~/Library/Audio/Plug-Ins/Components/

# Подписать
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/Components/Cohera\ Saturator.component

# Перезапустить Audio
killall -9 AudioComponentRegistrar coreaudiod 2>/dev/null
sleep 2

# Валидация
echo "=== VST3 Validation ==="
pluginval --strictness-level 5 --validate ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3

echo "=== AU Validation ==="
auval -v aufx Csat Cohr
```

---

## Часть 4: Сравнение VST3 vs AU

| Аспект                     | VST3                           | AU                                    |
|----------------------------|--------------------------------|---------------------------------------|
| **Подпись кода**           | Обязательна (ad-hoc минимум)   | Обязательна (ad-hoc минимум)          |
| **Идентификация**          | Bundle ID (`com.company.plugin`) | 4-char codes (`Cohr`, `Csat`, `aufx`) |
| **Регистрация**            | Gatekeeper при первом запуске  | AudioComponentRegistrar в фоне        |
| **Кэширование**            | DAW (Ableton, Logic)           | DAW + macOS AudioUnitCache            |
| **Валидация**              | `pluginval`                    | `auval -v aufx Csat Cohr`             |
| **Автогенерация метаданных** | Bundle ID из `COMPANY_NAME` + `PLUGIN_NAME` | ❌ Random codes если не указаны явно |

---

## Часть 5: Контрольный список для нового плагина

### ✅ Перед первой сборкой:
1. [ ] Зарегистрировать Manufacturer Code у Apple или выбрать 4 буквы
2. [ ] Выбрать уникальный Plugin Code (4 символа)
3. [ ] Добавить в `CMakeLists.txt`:
   ```cmake
   PLUGIN_MANUFACTURER_CODE Cohr
   PLUGIN_CODE Csat
   AU_MAIN_TYPE kAudioUnitType_Effect
   ```
4. [ ] Документировать коды в `README.md`:
   ```markdown
   ## AU Codes
   - Manufacturer: `Cohr` (CoheraAudio)
   - Plugin: `Csat` (Cohera Saturator)
   - Type: `aufx` (Audio Effect)
   ```

### ✅ После каждой сборки:
1. [ ] Подписать бинарники (`codesign --force --deep --sign -`)
2. [ ] Убрать quarantine (`xattr -cr`)
3. [ ] Скопировать в системные папки:
   - VST3: `~/Library/Audio/Plug-Ins/VST3/`
   - AU: `~/Library/Audio/Plug-Ins/Components/`
4. [ ] Очистить кэш DAW (Ableton Database, *.cfg)
5. [ ] Перезапустить аудио-службы:
   ```bash
   killall -9 AudioComponentRegistrar coreaudiod
   ```
6. [ ] Валидировать:
   ```bash
   pluginval --strictness-level 5 --validate <VST3_PATH>
   auval -v aufx Csat Cohr
   ```

### ✅ При проблемах:
1. [ ] Проверить подпись: `codesign -dv <PLUGIN_PATH>`
2. [ ] Проверить Info.plist AU: `plutil -p Info.plist | grep AudioComponents`
3. [ ] Проверить консоль macOS (Console.app) на ошибки codesign/AudioComponentRegistrar
4. [ ] Использовать `deep_clean_ableton.sh` (ядерный вариант)

---

## Часть 6: Ресурсы и инструменты

### Инструменты валидации:
- **pluginval** (VST3): https://github.com/Tracktion/pluginval
  ```bash
  brew install pluginval
  ```
- **auval** (AU): встроен в macOS
  ```bash
  auval -a  # список всех AU
  auval -v aufx Csat Cohr  # валидация конкретного плагина
  ```

### Документация:
- JUCE CMake API: https://github.com/juce-framework/JUCE/blob/master/docs/CMake%20API.md
- Apple Audio Unit Programming Guide: https://developer.apple.com/library/archive/documentation/MusicAudio/Conceptual/AudioUnitProgrammingGuide/
- Code Signing Guide: https://developer.apple.com/documentation/security/notarizing_macos_software_before_distribution

### Скрипты в проекте:
- `build_plugin.sh` — основная сборка (можно добавить автоподпись)
- `fix_ableton.sh` — установка + очистка кэша Ableton
- `deep_clean_ableton.sh` — ядерная очистка всех кэшей
- `check_plugins.sh` — проверка наличия файлов в системе
- `test_plugin.sh` — советы по тестированию в Ableton

---

## Резюме: Три главные ошибки и их решения

### 1. **"Ableton не видит VST3"**
**Причина:** Нет code signature  
**Решение:**
```bash
codesign --force --deep --sign - ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3
xattr -cr ~/Library/Audio/Plug-Ins/VST3/Cohera\ Saturator.vst3
```

### 2. **"auval не находит AU плагин"**
**Причина:** Неправильные 4-char codes (автогенерация JUCE)  
**Решение:**
```cmake
# В CMakeLists.txt:
PLUGIN_MANUFACTURER_CODE Cohr
PLUGIN_CODE Csat
AU_MAIN_TYPE kAudioUnitType_Effect
```

### 3. **"После пересборки ничего не изменилось"**
**Причина:** Кэш DAW или macOS  
**Решение:**
```bash
killall -9 "Ableton Live 11 Suite" AudioComponentRegistrar coreaudiod
rm -rf ~/Library/Preferences/Ableton/Live\ 11.*/Database/
rm -rf ~/Library/Caches/AudioUnitCache/
```

---

**Дата создания:** 2025-01-XX  
**Проект:** Cohera Saturator v1.30  
**Версия документа:** 1.0  
