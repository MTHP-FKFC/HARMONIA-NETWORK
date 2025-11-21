#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
#include "../dsp/DCBlocker.h"
#include "../dsp/InteractionEngine.h" // <--- ВАЖНО: Подключаем мозг

// Подключаем наши движки
#include "TransientEngine.h"
#include "AnalogModelingEngine.h"

namespace Cohera {

class BandProcessingEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        transientEngine.prepare(spec);
        analogEngine.prepare(spec);

        for(auto& dcb : dcBlockers) dcb.reset();
    }

    void reset()
    {
        transientEngine.reset();
        analogEngine.reset();
        for(auto& dcb : dcBlockers) dcb.reset();
    }

    // process обрабатывает блок аудио
    float process(juce::dsp::AudioBlock<float>& block,
                 const ParameterSet& params,
                 float driveTilt = 1.0f,
                 float netModulation = 0.0f)
    {
        // === 1. ВЫЧИСЛЕНИЕ СЕТЕВОЙ МОДУЛЯЦИИ ===
        // Спрашиваем у InteractionEngine, что и как крутить
        // params.netSens усиливает входящий сигнал модуляции
        ModulationTargets mods = InteractionEngine::calculateModulation(
            params.netMode,
            netModulation,
            params.netSens
        );

        // Применяем Depth (Глубину влияния) ко всем модуляторам
        float depth = params.netDepth;

        // === 2. ANALOG MODELING (MOJO) ===
        // Создаем копию параметров для модификации
        ParameterSet effectiveParams = params;

        // Применяем модуляцию к Mojo параметрам (Entropy Storm, Voltage Starve)
        if (std::abs(mods.mojoMod) > 0.001f) {
            effectiveParams.entropy += mods.mojoMod * depth;
            effectiveParams.analogDrift += mods.mojoMod * 0.5f * depth;
            effectiveParams.variance += mods.mojoMod * 0.5f * depth;

            // Клиппинг параметров 0..1
            effectiveParams.entropy = juce::jlimit(0.0f, 1.0f, effectiveParams.entropy);
        }

        auto [driveMultL, driveMultR] = analogEngine.process(block, effectiveParams);

        // === 3. РАСЧЕТ ДРАЙВА ===
        // Базовый множитель
        float combinedDriveMult = driveTilt * (driveMultL + driveMultR) * 0.5f;

        // A. Global Heat (Voltage Sag)
        if (params.globalHeat > 0.0f) {
            combinedDriveMult *= (1.0f + params.globalHeat * 0.2f);
        }

        // B. Network Drive Modulation (Unmasking, Ghost)
        if (std::abs(mods.driveMod) > 0.001f) {
            // driveMod может быть отрицательным (Ducking) или положительным (Boost)
            // Пример: -0.5 * depth -> уменьшаем драйв в 2 раза
            combinedDriveMult *= (1.0f + mods.driveMod * depth);
        }

        // Защита от отрицательного драйва
        combinedDriveMult = std::max(0.0f, combinedDriveMult);

        // === 4. ПРИМЕНЕНИЕ МОДУЛЯЦИИ К ДРУГИМ ПАРАМЕТРАМ ===

        // Transient Clone (Punch Boost)
        if (std::abs(mods.punchMod) > 0.001f) {
            effectiveParams.punch += mods.punchMod * depth;
            effectiveParams.punch = juce::jlimit(-1.0f, 1.0f, effectiveParams.punch);
        }

        // Harmonic Shield (Clean Blend)
        // Тут хитрость: blendMod управляет миксом внутри сатуратора?
        // SaturationEngine использует getSaturationBlend() из params.
        // Но мы не можем легко изменить drive внутри params так, чтобы blend изменился.
        // Поэтому SaturationEngine должен быть достаточно умен, но пока оставим как есть.
        // Для Harmonic Shield мы просто уменьшаем combinedDriveMult (это уже делает логика Unmasking).

        // === 5. TRANSIENT & SATURATION ===
        float maxTrans = transientEngine.process(block, effectiveParams, combinedDriveMult);

        // === 6. OUTPUT GAIN MODULATION (Gated, Unmasking) ===
        if (std::abs(mods.volumeMod) > 0.001f) {
            float volGain = 1.0f + mods.volumeMod * depth;
            volGain = std::max(0.0f, volGain); // Не инвертируем фазу
            block.multiplyBy(volGain);
        }

        // === 7. DC BLOCKER ===
        size_t numSamples = block.getNumSamples();
        size_t numChannels = block.getNumChannels();

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            if (ch >= 2) break;
            float* data = block.getChannelPointer(ch);
            for (size_t i = 0; i < numSamples; ++i)
            {
                data[i] = dcBlockers[ch].process(data[i]);
            }
        }

        return maxTrans;
    }

private:
    TransientEngine transientEngine;
    AnalogModelingEngine analogEngine;

    std::array<DCBlocker, 2> dcBlockers;
};

} // namespace Cohera
