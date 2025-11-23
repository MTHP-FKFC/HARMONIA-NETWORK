#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
#include "../dsp/MathSaturator.h"
#include "../dsp/ThermalModel.h" // Подключаем модель!

namespace Cohera {

class SaturationEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;

        smoothedDrive.reset(sampleRate, 0.05);
        smoothedBlend.reset(sampleRate, 0.05);

        smoothedDrive.setCurrentAndTargetValue(1.0f);
        smoothedBlend.setCurrentAndTargetValue(0.0f);

        // Готовим термо-модели для каждого канала (стерео-независимость)
        // Обычно макс 2 канала, но поддержим vector для сурраунда
        thermalModels.resize(spec.numChannels);
        for (auto& tm : thermalModels)
            tm.prepare(sampleRate);
    }

    void reset()
    {
        smoothedDrive.setCurrentAndTargetValue(1.0f);
        smoothedBlend.setCurrentAndTargetValue(0.0f);
        
        for (auto& tm : thermalModels)
            tm.reset();
    }
    
    // Геттер для UI (берем среднюю температуру по больнице/каналам)
    float getAverageTemperature() const
    {
        if (thermalModels.empty()) return 20.0f;
        float sum = 0.0f;
        for (const auto& tm : thermalModels) sum += tm.getCurrentTemp();
        return sum / (float)thermalModels.size();
    }
    
    // Геттер для сырого указателя (если нужен доступ к конкретному каналу)
    ThermalModel* getThermalModel(int channelIndex) 
    {
        if (channelIndex >= 0 && channelIndex < (int)thermalModels.size())
            return &thermalModels[channelIndex];
        return nullptr;
    }

    void process(juce::dsp::AudioBlock<float>& block, const ParameterSet& params)
    {
        float targetDrive = params.getEffectiveDriveGain();
        float targetBlend = params.getSaturationBlend();

        smoothedDrive.setTargetValue(targetDrive);
        smoothedBlend.setTargetValue(targetBlend);

        auto numSamples = block.getNumSamples();
        auto numChannels = block.getNumChannels();

        // Проверка на ресайз каналов (редкий кейс, но для безопасности)
        if (thermalModels.size() != numChannels)
        {
            thermalModels.resize(numChannels);
            for (auto& tm : thermalModels) tm.prepare(sampleRate);
        }

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            float* data = block.getChannelPointer(ch);
            // Получаем ссылку на модель конкретного канала
            auto& thermal = thermalModels[ch]; 

            for (size_t i = 0; i < numSamples; ++i)
            {
                // Поскольку Drive сглаживается медленно, можно вызывать getNextValue() 
                // один раз на сэмпл (снаружи цикла каналов), но внутри - точнее для стерео.
                // Оставим так для простоты чтения.
                float currentDrive = smoothedDrive.getNextValue(); 
                float currentBlend = smoothedBlend.getNextValue();
                
                float input = data[i];

                // --- ШАГ 1: ТЕРМОДИНАМИКА ---
                // Считаем нагрев и получаем смещение рабочей точки (Bias)
                // Bias добавляет асимметрию ДО сатурации -> четные гармоники
                float thermalBias = thermal.process(input);
                
                // Добавляем Bias к сигналу
                float biasedInput = input + thermalBias;

                // --- ШАГ 2: САТУРАЦИЯ ---
                // Сатурируем уже смещенный сигнал
                float saturated = mathSaturator.processSample(biasedInput, currentDrive, params.saturationMode);

                // --- ШАГ 3: CASCADE (Output Transformer) ---
                if (params.cascade)
                {
                    // Исправленная логика Soft Clip:
                    // Сначала мягко ограничиваем, потом жестко, чтобы не вылететь
                    // Формула: x * (1.5 - 0.5 * x^2) работает хорошо до 1.0
                    
                    // Жесткий клэмп перед полиномом не нужен, если полином правильный,
                    // но для безопасности оставим клэмп на входе в полином.
                    float temp = saturated;
                    if (temp > 1.0f) temp = 1.0f;
                    else if (temp < -1.0f) temp = -1.0f;
                    
                    saturated = temp * (1.5f - 0.5f * temp * temp);
                    saturated *= 1.1f; // Make-up gain транса
                }
                
                // Убираем постоянную составляющую (DC Bias), которую добавила лампа,
                // если мы хотим чистый выход (опционально, но лучше убрать)
                // saturated -= thermalBias; // Раскомментируй, если нужен чистый выход

                // --- ШАГ 4: BLEND ---
                data[i] = input + currentBlend * (saturated - input);
            }
        }
    }

private:
    double sampleRate = 44100.0;

    juce::SmoothedValue<float> smoothedDrive;
    juce::SmoothedValue<float> smoothedBlend;

    MathSaturator mathSaturator;
    
    // Массив тепловых моделей (по одной на канал)
    std::vector<ThermalModel> thermalModels;
};

} // namespace Cohera