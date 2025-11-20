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

    // Обновленная сигнатура: добавляем driveTilt
    // driveTilt: статический множитель (0.5 для Sub, 1.2 для Air и т.д.)
    // netModulation: динамический модулятор от сети (пока не используется, но передаем)
    void process(juce::dsp::AudioBlock<float>& block,
                 const ParameterSet& params,
                 float driveTilt = 1.0f,
                 float netModulation = 0.0f)
    {
        // 1. Analog Modeling (Mojo)
        auto [driveMultL, driveMultR] = analogEngine.process(block, params);

        // 2. Network Logic (Заглушка)
        // В будущем: float netFactor = ...logic(netModulation)...

        // 3. Итоговый множитель драйва
        // Tilt * (Average Analog Drift) * (Global Heat)
        float combinedDriveMult = driveTilt * (driveMultL + driveMultR) * 0.5f;

        // Global Heat (имитация просадки напряжения = буст драйва)
        if (params.globalHeat > 0.0f) {
            combinedDriveMult *= (1.0f + params.globalHeat * 0.2f);
        }

        // 4. Transient & Saturation
        transientEngine.process(block, params, combinedDriveMult);

        // 5. DC Blocker
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
