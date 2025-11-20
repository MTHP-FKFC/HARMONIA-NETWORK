#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "Envelope.h"

class AutoGainStage
{
public:
    void prepare(double sampleRate)
    {
        inputFollower.reset(sampleRate);
        outputFollower.reset(sampleRate);

        // Сглаживание компенсации (50мс)
        smoothedComp.reset(sampleRate, 0.05);
        smoothedComp.setCurrentAndTargetValue(1.0f);
    }

    // Вспомогательная функция: RMS с HPF фильтром
    float getWeightedRMS(const juce::AudioBuffer<float>& buffer)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = buffer.getNumChannels();
        double sumSquares = 0.0;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            float prev = data[0];
            for (int i = 1; i < numSamples; ++i)
            {
                float current = data[i];
                float filtered = current - prev; // HPF дифференциатор
                sumSquares += (filtered * filtered);
                prev = current;
            }
        }

        return (float)std::sqrt(sumSquares / (numSamples * numCh));
    }

    // Шаг 1: Анализ входа (вызывать до обработки)
    void analyzeInput(const juce::AudioBuffer<float>& buffer)
    {
        float energy = getWeightedRMS(buffer);
        if (energy < 0.00001f) energy = 0.00001f;
        currentInLevel = inputFollower.process(energy);
    }

    // Шаг 2: Анализ выхода и обновление компенсации (вызывать после обработки)
    void updateGainState(const juce::AudioBuffer<float>& buffer)
    {
        float energy = getWeightedRMS(buffer);
        if (energy < 0.00001f) energy = 0.00001f;
        float currentOutLevel = outputFollower.process(energy);

        // Вычисляем цель
        float target = 1.0f;
        if (currentOutLevel > 0.001f && currentInLevel > 0.001f)
        {
            // Добавляем небольшой буст +10% (1.1f), так как RMS матчинг часто звучит субъективно тише пикового
            target = (currentInLevel / currentOutLevel) * 1.1f;
        }

        // Лимиты безопасности (+12dB / -24dB)
        target = juce::jlimit(0.06f, 4.0f, target);

        smoothedComp.setTargetValue(target);
    }

    // Шаг 3: Применение (вызывать внутри processBlock на Wet буфер)
    // Возвращает множитель для текущего сэмпла (или блока)
    float getNextValue()
    {
        return smoothedComp.getNextValue();
    }

    // Получить текущий уровень входа (для сети)
    float getCurrentInputLevel() const { return currentInLevel; }

private:
    EnvelopeFollower inputFollower;
    EnvelopeFollower outputFollower;
    juce::LinearSmoothedValue<float> smoothedComp;

    float currentInLevel = 0.0f;
};
