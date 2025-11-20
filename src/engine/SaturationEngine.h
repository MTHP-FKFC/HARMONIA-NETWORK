#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
#include "../dsp/MathSaturator.h" // Ваш существующий файл

namespace Cohera {
using namespace Cohera;

class SaturationEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        // Настраиваем сглаживание (50ms response)
        smoothedDrive.reset(sampleRate, 0.05);
        smoothedBlend.reset(sampleRate, 0.05);

        // Ставим начальные значения, чтобы не было "въезда" звука при старте
        smoothedDrive.setCurrentAndTargetValue(1.0f);
        smoothedBlend.setCurrentAndTargetValue(0.0f);
    }

    void reset()
    {
        smoothedDrive.setCurrentAndTargetValue(1.0f);
        smoothedBlend.setCurrentAndTargetValue(0.0f);
    }

    // Основной метод обработки
    // Важно: мы не меняем block, а обрабатываем in-place
    void process(juce::dsp::AudioBlock<float>& block, const ParameterSet& params)
    {
        // 1. Обновляем цели сглаживания из ParameterSet
        float targetDrive = params.getEffectiveDriveGain();
        float targetBlend = params.getSaturationBlend();

        smoothedDrive.setTargetValue(targetDrive);
        smoothedBlend.setTargetValue(targetBlend);

        // 2. Посэмпловая обработка
        auto numSamples = block.getNumSamples();
        auto numChannels = block.getNumChannels();

        for (size_t i = 0; i < numSamples; ++i)
        {
            // Получаем сглаженные значения для текущего сэмпла
            float currentDrive = smoothedDrive.getNextValue();
            float currentBlend = smoothedBlend.getNextValue();

            // Оптимизация: Если blend ~ 0, пропускаем тяжелую математику?
            // Для MathSaturator::WarmTube (tanh) это мб не критично,
            // но для SuperEllipse - полезно.
            // Пока делаем честно.

            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                float* data = block.getChannelPointer(ch);
                float input = data[i];

                // Вызов вашего MathSaturator
                // Обратите внимание: мы передаем чистый MathMode из params
                float saturated = mathSaturator.processSample(input, currentDrive, params.mathMode);

                // Dry/Wet blend внутри модуля сатурации (логика "Чистого нуля")
                // Если Blend < 1.0, миксуем (input) и (saturated)
                // Если Blend == 1.0, результат полностью saturated
                data[i] = input + currentBlend * (saturated - input);
            }
        }
    }

private:
    double sampleRate = 44100.0;

    // Сглаживатели (чтобы не трещало при кручении ручек)
    juce::SmoothedValue<float> smoothedDrive;
    juce::SmoothedValue<float> smoothedBlend;

    // Ваш класс математики (без изменений, только include)
    MathSaturator mathSaturator;
};

} // namespace Cohera
