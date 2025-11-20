#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
// Подключаем DSP модули
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

    // Возвращает {L_mult, R_mult} для драйва и модифицирует входной блок (добавляет Bias)
    std::pair<float, float> process(juce::dsp::AudioBlock<float>& block, const ParameterSet& params)
    {
        smoothAnalogDrift.setTargetValue(params.analogDrift);
        smoothEntropy.setTargetValue(params.entropy);
        smoothVariance.setTargetValue(params.variance);

        size_t numSamples = block.getNumSamples();
        size_t numChannels = block.getNumChannels();

        // Получаем параметры (раз в блок, для экономии на LFO)
        float driftAmount = smoothAnalogDrift.getNextValue();
        float entropyAmount = smoothEntropy.getNextValue();
        float varAmount = smoothVariance.getNextValue();

        // Рассчитываем дрейф драйва (Stereo Variance)
        auto driftValues = varianceModule.getDrift(varAmount);

        for (size_t i = 0; i < numSamples; ++i)
        {
            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                if (ch >= 2) break;

                float* data = block.getChannelPointer(ch);
                float x = data[i];

                // 1. Thermal Bias (Ламповый нагрев от сигнала)
                float bias = tubes[ch].process(x) * driftAmount;

                // 2. Entropy (Случайный дрейф рабочей точки)
                float entropy = entropyModules[ch].process(entropyAmount);

                // Складываем: Input + Bias
                data[i] = x + bias + entropy;
            }
        }

        return { driftValues.driveMultL, driftValues.driveMultR };
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
