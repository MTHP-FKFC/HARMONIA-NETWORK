#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
#include "../dsp/MathSaturator.h"
// #include "../dsp/TransientSplitter.h" // TEMPORARILY COMMENTED

namespace Cohera {

class TransientEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        // Для каждого канала свой сплиттер
        // for (auto& s : splitters) {
        //     s.prepare(spec.sampleRate);
        //     s.reset();
        // }

        // Сглаживание панча
        smoothedPunch.reset(spec.sampleRate, 0.05);
        smoothedPunch.setCurrentAndTargetValue(0.0f);
    }

    void reset()
    {
        // for (auto& s : splitters) s.reset();
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

            // TEMPORARY: Simple processing without Split & Crush for compilation test
            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                float* data = block.getChannelPointer(ch);
                float input = data[i];

                // Простая сатурация без Split & Crush
                data[i] = mathSaturator.processSample(input, baseDrive, params.mathMode);
            }
        }
    }

private:
    // Сплиттеры для стерео (макс 2 канала)
    // std::array<TransientSplitter, 2> splitters; // TEMPORARILY COMMENTED
    MathSaturator mathSaturator;

    juce::SmoothedValue<float> smoothedPunch;
};

} // namespace Cohera
