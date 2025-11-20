#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
#include "../dsp/MathSaturator.h"
#include "../dsp/TransientSplitter.h"

namespace Cohera {

class TransientEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        // Для каждого канала свой сплиттер
        for (auto& s : splitters) {
            s.prepare(spec.sampleRate);
            s.reset();
        }

        // Сглаживание панча
        smoothedPunch.reset(spec.sampleRate, 0.05);
        smoothedPunch.setCurrentAndTargetValue(0.0f);
    }

    void reset()
    {
        for (auto& s : splitters) s.reset();
        smoothedPunch.setCurrentAndTargetValue(0.0f);
    }

    // Основной метод. Заменяет обычную сатурацию, добавляя логику транзиентов.
    // Принимает сэмпл (или указатель), возвращает обработанный.
    // Работает IN-PLACE над AudioBlock.
    void process(juce::dsp::AudioBlock<float>& block, const ParameterSet& params, float driveMult = 1.0f)
    {
        smoothedPunch.setTargetValue(params.punch);

        // Базовый драйв с учетом всех модуляций
        float baseDrive = params.getEffectiveDriveGain() * driveMult;

        auto numSamples = block.getNumSamples();
        auto numChannels = block.getNumChannels();

        for (size_t i = 0; i < numSamples; ++i)
        {
            float punchVal = smoothedPunch.getNextValue();

            // Оптимизация: если Punch около 0, работаем в режиме обычной сатурации (без сплита)
            // Это экономит CPU, пропуская TransientSplitter
            bool isNeutral = std::abs(punchVal) < 0.01f;

            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                float* data = block.getChannelPointer(ch);
                float input = data[i];

                if (isNeutral)
                {
                    // Классический режим: просто сатурация
                    data[i] = mathSaturator.processSample(input, baseDrive, params.mathMode);
                }
                else
                {
                    // Режим Split & Crush
                    auto split = splitters[ch].process(input);

                    // 1. Body всегда жирное (основной алгоритм)
                    float processedBody = mathSaturator.processSample(split.body, baseDrive, params.mathMode);

                    // 2. Transient зависит от режима Punch
                    float processedTrans = 0.0f;

                    if (punchVal > 0.0f) // Positive: Dirty Attack
                    {
                        float transDrive = baseDrive * (1.0f + punchVal * 2.0f);
                        processedTrans = mathSaturator.processSample(split.trans, transDrive, params.mathMode);
                    }
                    else // Negative: Clean Attack
                    {
                        float transDrive = baseDrive * (1.0f - std::abs(punchVal) * 0.8f);
                        // Используем EulerTube для мягкости на атаках в Clean режиме
                        processedTrans = mathSaturator.processSample(split.trans, transDrive, MathMode::EulerTube);
                    }

                    // Сумма
                    data[i] = processedBody + processedTrans;
                }
            }
        }
    }

private:
    // Сплиттеры для стерео (макс 2 канала)
    std::array<TransientSplitter, 2> splitters;
    MathSaturator mathSaturator;

    juce::SmoothedValue<float> smoothedPunch;
};

} // namespace Cohera
