#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
#include "../dsp/ThermalModel.h"
#include "../dsp/HarmonicEntropy.h"
#include "../dsp/StereoVariance.h"

namespace Cohera {

class AnalogModelingEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        for (int ch = 0; ch < 2; ++ch) {
            tubes[ch].prepare(spec.sampleRate);
            tubes[ch].reset();
            entropyModules[ch].prepare(spec.sampleRate);
            entropyModules[ch].reset();
        }

        varianceModule.prepare(spec.sampleRate);

        // Сглаживание параметров
        smoothAnalogDrift.reset(spec.sampleRate, 0.05);
        smoothEntropy.reset(spec.sampleRate, 0.05);
        smoothVariance.reset(spec.sampleRate, 0.05);
    }

    void reset()
    {
        for (int ch = 0; ch < 2; ++ch) {
            tubes[ch].reset();
            entropyModules[ch].reset();
        }
        smoothAnalogDrift.setCurrentAndTargetValue(0.0f);
    }

    // Возвращает множитель драйва (может быть разным для L/R из-за Variance)
    // И модифицирует входной буфер (добавляет Bias)
    std::pair<float, float> process(juce::dsp::AudioBlock<float>& block, const ParameterSet& params)
    {
        smoothAnalogDrift.setTargetValue(params.analogDrift);
        smoothEntropy.setTargetValue(params.entropy);
        smoothVariance.setTargetValue(params.variance);

        auto numSamples = block.getNumSamples();

        // Получаем текущие значения параметров (берем первое значение на блок для простотыVariance/Drift)
        // Для супер-точности можно делать внутри цикла, но variance обычно медленный LFO.
        float driftAmount = smoothAnalogDrift.getNextValue();
        float entropyAmount = smoothEntropy.getNextValue();
        float varAmount = smoothVariance.getNextValue();

        // Расчет стерео разброса драйва
        auto driftmults = varianceModule.getDrift(varAmount);

        for (size_t i = 0; i < numSamples; ++i)
        {
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                float* data = block.getChannelPointer(ch);
                float x = data[i];

                // 1. Thermal Bias (Ламповый нагрев)
                float bias = tubes[ch].process(x) * driftAmount;

                // 2. Entropy (Стохастический дрейф)
                float entropy = entropyModules[ch].process(entropyAmount);

                // Добавляем смещение к сигналу
                data[i] = x + bias + entropy;
            }
        }

        return { driftmults.driveMultL, driftmults.driveMultR };
    }

private:
    std::array<ThermalModel, 2> tubes;
    std::array<HarmonicEntropy, 2> entropyModules;
    StereoVariance varianceModule;

    juce::SmoothedValue<float> smoothAnalogDrift;
    juce::SmoothedValue<float> smoothEntropy;
    juce::SmoothedValue<float> smoothVariance;
};

} // namespace Cohera
