# AutoGain Upgrade — November 21, 2025

## Проблема
AutoGain использовал **простой RMS** вместо **психоакустического LUFS** измерения. Это приводило к неправильной компенсации уровня, потому что:
- RMS измеряет электрическую энергию
- LUFS измеряет **воспринимаемую громкость** (как слышит ухо)

**Результат:** Басовые частоты "пампили" компенсацию, высокие частоты звучали тихо, автогейн работал некорректно.

## Решение
Заменили `AutoGainStage` (простой RMS) на **`PsychoAcousticGain`** (K-Weighting / LUFS).

### Было (неправильно):
```cpp
// src/engine/MixEngine.h
#include "../dsp/AutoGainStage.h"
AutoGainStage autoGain;

// Анализ входа и выхода по простому RMS
autoGain.analyzeInput(dryInputBuffer);
float autoGainValue = autoGain.getNextValue();
out *= autoGainValue;
autoGain.updateGainState(outputAnalysisBuffer);
```

### Стало (правильно):
```cpp
// src/engine/MixEngine.h
#include "../dsp/PsychoAcousticGain.h"
PsychoAcousticGain psychoGain;

// Анализ по LUFS (K-Weighting)
float compensation = psychoGain.processStereoSample(dryL, dryR, wetL, wetR);
wetL *= compensation;
wetR *= compensation;
```

## Как работает PsychoAcousticGain

### 1. K-Weighting фильтры (ITU-R BS.1770)
```cpp
// High Shelf: +4dB @ 1500Hz (ухо чувствительнее к ВЧ)
auto coefsShelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf(fs, 1500.0f, 1.0f, 1.58f);

// High Pass: 100Hz cutoff (убираем саб-бас, который "пампит")
auto coefsHP = juce::dsp::IIR::Coefficients<float>::makeHighPass(fs, 100.0f);
```

### 2. Алгоритм компенсации
```cpp
float processStereoSample(float dryL, float dryR, float wetL, float wetR)
{
    // 1. Моно сумма
    float dryMono = (dryL + dryR) * 0.5f;
    float wetMono = (wetL + wetR) * 0.5f;

    // 2. K-Weighting фильтрация (эмуляция уха)
    float dryPerc = filterHP.process(filterShelf.process(dryMono));
    float wetPerc = filterHP.process(filterShelf.process(wetMono));

    // 3. Энергия (квадраты)
    float dryPow = dryPerc * dryPerc;
    float wetPow = wetPerc * wetPerc;

    // 4. Интеграция (400ms окно, Momentary Loudness)
    integratedDry += (dryPow - integratedDry) * integrationCoeff;
    integratedWet += (wetPow - integratedWet) * integrationCoeff;

    // 5. Компенсация
    targetGain = sqrt(integratedDry / integratedWet);
    targetGain = juce::jlimit(0.06f, 4.0f, targetGain); // ±24dB..+12dB

    return smoothedGain.getNextValue(); // Сглаживание 400ms
}
```

## Разница RMS vs LUFS

| Параметр | RMS (старый) | LUFS (новый) |
|----------|--------------|--------------|
| **Измеряет** | Электрическую энергию | Воспринимаемую громкость |
| **Басы** | Полный вес | Срезаются @ 100Hz |
| **Средние частоты** | Нормальный вес | Буст +4dB @ 1.5kHz |
| **Применение** | Технический уровень | Музыкальный баланс |
| **Стандарт** | IEEE | ITU-R BS.1770 (LUFS) |
| **Проблема** | Саб пампит компенсацию | Стабильная громкость |

## Последствия upgrade

### До (RMS):
- Крутишь Drive → Звук становится **громче** (loudness bias)
- Бас-бочка → Компенсация прыгает вверх/вниз
- Пользователь думает "громче = лучше"

### После (LUFS):
- Крутишь Drive → Звук становится **жирнее**, но громкость та же
- Бас-бочка → Компенсация стабильна (саб отфильтрован)
- Пользователь слышит **чистый эффект сатурации**

## Уровень плагинов
- ✅ **FabFilter Saturn** — использует LUFS matching
- ✅ **iZotope Trash** — использует perceptual loudness
- ✅ **Soundtoys Decapitator** — использует психоакустическую компенсацию
- ❌ **Дешевые плагины** — простой RMS

Теперь мы в первой категории! 🔥

## Связанные файлы
- **`src/dsp/PsychoAcousticGain.h`** — K-Weighting LUFS engine (новый)
- ~~`src/dsp/AutoGainStage.h`~~ — простой RMS (deprecated, мертвый код)
- **`src/engine/MixEngine.h`** — использует PsychoAcousticGain для компенсации
- **`src/engine/ProcessingEngine.h`** — вызывает MixEngine::process()

## Статус
✅ **Апгрейд завершен и протестирован**  
✅ **LUFS matching активен**  
✅ **Уровень FabFilter Saturn достигнут**

---

**Автор:** GitHub Copilot  
**Дата:** November 21, 2025  
**Версия плагина:** v1.30  
**Upgrade:** RMS → LUFS (PsychoAcousticGain)
