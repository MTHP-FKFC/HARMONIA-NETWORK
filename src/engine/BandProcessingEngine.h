#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
#include "../dsp/DCBlocker.h"

// Подключаем наши восстановленные движки
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

    // Главный метод обработки полосы
    void process(juce::dsp::AudioBlock<float>& block,
                 const ParameterSet& params,
                 float netModulation = 0.0f)
    {
        // 1. Analog Modeling (Mojo)
        // Возвращает множители драйва для L/R каналов
        auto [driveMultL, driveMultR] = analogEngine.process(block, params);

        // 2. Network Logic Stub (Пока заглушка, но место готово)
        // netModulation придет извне (от ProcessingEngine)

        // 3. Рассчитываем средний множитель драйва
        float avgDriveMult = (driveMultL + driveMultR) * 0.5f;

        // Global Heat (имитация просадки питания)
        if (params.globalHeat > 0.0f) {
            avgDriveMult *= (1.0f + params.globalHeat * 0.2f);
        }

        // 4. Transient & Saturation
        // Применяем сатурацию с учетом транзиентов
        transientEngine.process(block, params, avgDriveMult);

        // 5. DC Blocker (убираем постоянку после нелинейности)
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
    }

private:
    TransientEngine transientEngine;
    AnalogModelingEngine analogEngine;

    std::array<DCBlocker, 2> dcBlockers;
};

} // namespace Cohera
