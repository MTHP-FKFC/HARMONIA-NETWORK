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

    // Шаг 1: Анализ входа (вызывать до обработки)
    void analyzeInput(const juce::AudioBuffer<float>& buffer)
    {
        float energy = 0.0f;
        const int numCh = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        for (int ch = 0; ch < numCh; ++ch)
            energy += buffer.getMagnitude(ch, 0, numSamples);

        currentInLevel = inputFollower.process(energy / (float)numCh);
    }

    // Шаг 2: Анализ выхода и обновление компенсации (вызывать после обработки)
    void updateGainState(const juce::AudioBuffer<float>& buffer)
    {
        float energy = 0.0f;
        const int numCh = buffer.getNumChannels();
        const int numSamples = buffer.getNumSamples();

        for (int ch = 0; ch < numCh; ++ch)
            energy += buffer.getMagnitude(ch, 0, numSamples);

        float currentOutLevel = outputFollower.process(energy / (float)numCh);

        // Вычисляем цель
        float target = 1.0f;
        if (currentOutLevel > 0.001f && currentInLevel > 0.001f)
        {
            target = currentInLevel / currentOutLevel;
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

private:
    EnvelopeFollower inputFollower;
    EnvelopeFollower outputFollower;
    juce::LinearSmoothedValue<float> smoothedComp;

    float currentInLevel = 0.0f;
};
