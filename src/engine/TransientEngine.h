#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
// Подключаем DSP модули
#include "../dsp/MathSaturator.h"
#include "../dsp/TransientSplitter.h"

namespace Cohera {

class TransientEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        for (auto& s : splitters) {
            s.prepare(spec.sampleRate);
            s.reset();
        }

        smoothedPunch.reset(spec.sampleRate, 0.05);
        smoothedPunch.setCurrentAndTargetValue(0.0f);
    }

    void reset()
    {
        for (auto& s : splitters) s.reset();
        smoothedPunch.setCurrentAndTargetValue(0.0f);
    }

    // Основной процессинг
    void process(juce::dsp::AudioBlock<float>& block, const ParameterSet& params, float driveMult = 1.0f)
    {
        smoothedPunch.setTargetValue(params.punch);

        // Эффективный драйв (база * модуляция извне)
        float baseDrive = params.getEffectiveDriveGain() * driveMult;

        size_t numSamples = block.getNumSamples();
        size_t numChannels = block.getNumChannels();

        for (size_t i = 0; i < numSamples; ++i)
        {
            float punchVal = smoothedPunch.getNextValue();
            bool isNeutral = std::abs(punchVal) < 0.01f;

            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                // Защита от выхода за пределы массива сплиттеров (у нас их 2)
                if (ch >= 2) break;

                float* data = block.getChannelPointer(ch);
                float input = data[i];

                if (isNeutral)
                {
                    // 1. Просто Сатурация (экономим CPU на сплите)
                    data[i] = mathSaturator.processSample(input, baseDrive, params.saturationMode);
                }
                else
                {
                    // 2. Split & Crush
                    auto split = splitters[ch].process(input);

                    // Body: всегда основной алгоритм
                    float processedBody = mathSaturator.processSample(split.body, baseDrive, params.saturationMode);

                    // Transient: зависит от знака Punch
                    float processedTrans = 0.0f;

                    if (punchVal > 0.0f) // Dirty Attack
                    {
                        float transDrive = baseDrive * (1.0f + punchVal * 2.0f);
                        processedTrans = mathSaturator.processSample(split.trans, transDrive, params.saturationMode);
                    }
                    else // Clean Attack
                    {
                        float transDrive = baseDrive * (1.0f - std::abs(punchVal) * 0.8f);
                        // Для чистоты используем EulerTube (или Clean, если нужно совсем прозрачно)
                        processedTrans = mathSaturator.processSample(split.trans, transDrive, SaturationMode::EulerTube);
                    }

                    // Сумма
                    data[i] = processedBody + processedTrans;
                }
            }
        }
    }

private:
    std::array<TransientSplitter, 2> splitters;
    MathSaturator mathSaturator;
    juce::SmoothedValue<float> smoothedPunch;
};

} // namespace Cohera
