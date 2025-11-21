#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

class AutoGainStage
{
public:
    void prepare(double sampleRate)
    {
        fs = sampleRate;
        // 300ms - стандарт VU метра (человеческое восприятие громкости)
        rmsCoeff = 1.0f - std::exp(-1.0f / (0.3f * (float)fs));

        // Ускоряем реакцию компенсации (было 100мс, ставим 50мс),
        // чтобы быстрее ловить резкие скачки баса
        smoothedComp.reset(sampleRate, 0.05);
        smoothedComp.setCurrentAndTargetValue(1.0f);

        resetStates();
    }

    void resetStates()
    {
        accumulatorIn = 0.0f;
        accumulatorOut = 0.0f;
        currentInEnergy = 0.0f;
        currentOutEnergy = 0.0f;
    }

    // Шаг 1: Анализ ВХОДА (Full Bandwidth RMS)
    void analyzeInput(const juce::AudioBuffer<float>& buffer)
    {
        currentInEnergy = measureRMS(buffer, true);
    }

    // Шаг 2: Анализ ВЫХОДА
    void updateGainState(const juce::AudioBuffer<float>& buffer)
    {
        currentOutEnergy = measureRMS(buffer, false);

        float target = 1.0f;

        // Порог тишины (чтобы не бустить шум паузы)
        if (currentOutEnergy > 0.00001f && currentInEnergy > 0.00001f)
        {
            // Сравниваем амплитуды (корень из энергии)
            target = std::sqrt(currentInEnergy / currentOutEnergy);
        }

        // Жесткие лимиты: не даем делать громче +6dB и тише -24dB
        target = juce::jlimit(0.06f, 2.0f, target);

        smoothedComp.setTargetValue(target);
    }

    float getNextValue()
    {
        return smoothedComp.getNextValue();
    }

private:
    // Честный RMS без фильтрации частот (чтобы видеть Саб-бас!)
    float measureRMS(const juce::AudioBuffer<float>& buffer, bool updateState)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = juce::jmin(buffer.getNumChannels(), 2);

        double sumSquares = 0.0;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* data = buffer.getReadPointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                float x = data[i];
                sumSquares += (x * x);
            }
        }

        double meanSquare = sumSquares / (double)(numSamples * numCh);

        // Интегрируем во времени (сглаживание)
        float& accumulator = updateState ? accumulatorIn : accumulatorOut;
        accumulator = accumulator * rmsCoeff + (float)meanSquare * (1.0f - rmsCoeff);

        // Возвращаем мгновенную энергию (чтобы не ждать разгона аккумулятора)
        // Или аккумулятор? Для стабильности лучше аккумулятор.
        return accumulator;
    }

    double fs = 44100.0;
    float rmsCoeff = 0.0f;

    float accumulatorIn = 0.0f;
    float accumulatorOut = 0.0f;

    float currentInEnergy = 0.0f;
    float currentOutEnergy = 0.0f;

    juce::LinearSmoothedValue<float> smoothedComp;
};
