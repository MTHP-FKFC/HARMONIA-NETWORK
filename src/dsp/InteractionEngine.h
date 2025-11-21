#pragma once

#include "../CoheraTypes.h"

// === MODULATION TARGETS ===
// Структура, которая говорит системе, КАКОЙ параметр модулировать
struct ModulationTargets {
    float driveMod = 0.0f;    // +/- Drive (общий множитель)
    float volumeMod = 0.0f;   // +/- Output Gain (громкость)
    float punchMod = 0.0f;    // +/- Punch (транзиенты)
    float filterMod = 0.0f;   // +/- Filter Cutoff (Tighten/Smooth)
    float mojoMod = 0.0f;     // +/- Mojo (Drift/Entropy/Heat)
    float blendMod = 0.0f;    // +/- Dry/Wet Blend (чистота/грязь)

    // Вспомогательные методы
    void reset() {
        driveMod = volumeMod = punchMod = filterMod = mojoMod = blendMod = 0.0f;
    }

    bool isActive() const {
        return std::abs(driveMod) > 0.001f || std::abs(volumeMod) > 0.001f ||
               std::abs(punchMod) > 0.001f || std::abs(filterMod) > 0.001f ||
               std::abs(mojoMod) > 0.001f || std::abs(blendMod) > 0.001f;
    }
};

class InteractionEngine
{
public:
    // === MAIN MODULATION CALCULATOR ===
    // inputEnvelope: 0.0 .. 1.0 (сигнал от Reference)
    // sensitivity: 0.0 .. 2.0 (ручка Sens)
    static ModulationTargets calculateModulation(Cohera::NetworkMode mode, float inputEnvelope, float sensitivity)
    {
        ModulationTargets targets;
        float signal = inputEnvelope * sensitivity;

        switch (mode) {
            // === CLASSIC MIXING MODES ===

            case Cohera::NetworkMode::Unmasking: // 0: "Освободи место"
                // Reference громкий -> Listener тихий + меньше драйва
                targets.driveMod = -0.5f * signal;
                targets.volumeMod = -1.0f * signal;
                break;

            case Cohera::NetworkMode::Ghost: // 1: "Синхронная энергия"
                // Reference громкий -> Listener получает больше сатурации
                targets.driveMod = 1.0f * signal;
                break;

            case Cohera::NetworkMode::Gated: // 2: "Играй в паузах"
                // Reference громкий -> Listener молчит
                targets.volumeMod = -1.0f * signal;
                break;

            case Cohera::NetworkMode::StereoBloom: // 3: "Пространственный взрыв"
                // Reference громкий -> Listener расширяется (Focus в Side)
                // NOTE: Это обрабатывается в StereoEngine, здесь только драйв
                targets.driveMod = 0.3f * signal;
                break;

            case Cohera::NetworkMode::Sympathetic: // 4: "Резонанс"
                // Reference громкий -> Listener насыщается гармониками
                targets.driveMod = 0.8f * signal;
                targets.mojoMod = 0.5f * signal; // Буст энтропии
                break;

            // === ADVANCED MIXING MODES ===

            case Cohera::NetworkMode::TransientClone: // 5: "Заимствование Атаки"
                // Reference бьет -> Listener получает резкий буст Punch
                targets.punchMod = 1.0f * signal;
                break;

            case Cohera::NetworkMode::SpectralSculpt: // 6: "Динамический Эквалайзер"
                // Reference громкий -> Listener сдвигает фильтры (Tighten вверх)
                targets.filterMod = 1.0f * signal;
                break;

            case Cohera::NetworkMode::VoltageStarve: // 7: "Энергетический Вампиризм"
                // Reference громкий -> У Listener'а падает "напряжение"
                targets.mojoMod = 1.0f * signal;  // Heat вызывает просадку
                targets.driveMod = 0.2f * signal; // Немного грязи
                break;

            case Cohera::NetworkMode::EntropyStorm: // 8: "Управляемый Хаос"
                // Reference активен -> Listener получает максимум хаоса
                targets.mojoMod = 1.0f * signal; // Drift + Entropy + Variance
                break;

            case Cohera::NetworkMode::HarmonicShield: // 9: "Анти-Сатурация"
                // Reference громкий -> Listener становится ЧИЩЕ
                targets.blendMod = -1.0f * signal; // Убираем Wet (чистота)
                break;
        }

        return targets;
    }

    // === LEGACY SUPPORT ===
    // Для обратной совместимости с старой системой (если нужно)
    struct DualShaperConfig {
        int typeA = 0;
        float driveScaleA = 1.0f;
        int typeB = 0;
        float driveScaleB = 1.0f;
    };

    static DualShaperConfig getConfiguration(int modeIndex, int bandIndex, int userSelectedType) {
        // Преобразование старых индексов в новые режимы
        Cohera::NetworkMode mode = static_cast<Cohera::NetworkMode>(modeIndex);

        // Для legacy: возвращаем базовую конфигурацию без модуляции
        DualShaperConfig cfg { userSelectedType, 1.0f, userSelectedType, 1.0f };
        return cfg;
    }
};
