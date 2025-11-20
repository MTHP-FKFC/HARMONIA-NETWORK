#pragma once
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include <algorithm>

class AutoGainStage
{
public:
    void prepare(double sampleRate)
    {
        fs = sampleRate;

        // Настройка HPF фильтра (Cutoff ~100Hz для детектора)
        // y[n] = x[n] - x[n-1] + coeff * y[n-1]
        // coeff ~ 0.985 для 100Hz при 44.1k
        hpfCoeff = std::exp(-2.0f * juce::MathConstants<float>::pi * 100.0f / (float)fs);

        // Настройка RMS усреднителя (300ms окно интеграции - стандарт VU метра)
        rmsCoeff = 1.0f - std::exp(-1.0f / (0.3f * (float)fs));

        // Сброс состояний
        resetStates();

        // Сглаживание самой компенсации (чтобы гейн не дрожал)
        smoothedComp.reset(sampleRate, 0.1); // 100ms
        smoothedComp.setCurrentAndTargetValue(1.0f);
    }

    void resetStates()
    {
        // Сбрасываем память фильтров для каждого канала
        for (auto& v : filterStateX) v = 0.0f;
        for (auto& v : filterStateY) v = 0.0f;
        currentInEnergy = 0.0f;
        currentOutEnergy = 0.0f;
    }

    // Шаг 1: Анализ ВХОДА (True RMS + HPF)
    void analyzeInput(const juce::AudioBuffer<float>& buffer)
    {
        currentInEnergy = measureFilteredRMS(buffer, true); // true = обновлять состояние фильтров
    }

    // Шаг 2: Анализ ВЫХОДА и расчет компенсации
    void updateGainState(const juce::AudioBuffer<float>& buffer)
    {
        // Для выхода используем те же фильтры, но состояние не храним так строго,
        // так как выход меняется. Но для симметрии лучше использовать отдельное состояние или сброс.
        // Для простоты и скорости здесь используем простой RMS без фильтрации,
        // ЛИБО (лучше) тот же алгоритм, но с отдельным состоянием.

        // Чтобы не плодить память, измерим просто RMS выхода (без HPF),
        // но с поправкой на то, что вход был фильтрован.
        // *ХАК*: Сатурация добавляет гармоники ВВЕРХУ. HPF на входе убрал низ.
        // Если мы сравним FilteredInput vs FullOutput, выход будет казаться громче (там есть низ).
        // Поэтому выход тоже надо фильтровать, иначе бас будет проваливаться.

        // Используем временные переменные для фильтрации выхода (stateless в рамках блока, но это ок для RMS)
        currentOutEnergy = measureFilteredRMS(buffer, false);

        // Вычисляем цель
        float target = 1.0f;

        // Порог тишины -60dB
        if (currentOutEnergy > 0.0001f && currentInEnergy > 0.0001f)
        {
            // Сравниваем Энергию (квадраты), потом берем корень -> получаем соотношение амплитуд
            target = std::sqrt(currentInEnergy / currentOutEnergy);
        }

        // Лимиты безопасности (+12dB / -12dB)
        // Не даем разгонять слишком сильно, чтобы не вытаскивать шум
        target = juce::jlimit(0.25f, 4.0f, target);

        smoothedComp.setTargetValue(target);
    }

    float getNextValue()
    {
        return smoothedComp.getNextValue();
    }

private:
    // Измеряет RMS, предварительно отрезав низ (чтобы бочка не пампила микс)
    float measureFilteredRMS(const juce::AudioBuffer<float>& buffer, bool updateState)
    {
        const int numSamples = buffer.getNumSamples();
        const int numCh = juce::jmin(buffer.getNumChannels(), 2);

        double sumSquares = 0.0;

        for (int ch = 0; ch < numCh; ++ch)
        {
            const float* data = buffer.getReadPointer(ch);

            // Локальные копии состояний, чтобы не портить их, если updateState=false
            float x1 = filterStateX[ch];
            float y1 = filterStateY[ch];

            for (int i = 0; i < numSamples; ++i)
            {
                float x0 = data[i];

                // Однополюсный HPF: y[n] = x[n] - x[n-1] + coef * y[n-1]
                // Этот фильтр убирает DC и Саб-низ.
                // Коэффициент 0.995 ~ 80-100Hz
                float y0 = x0 - x1 + 0.995f * y1;

                // Накапливаем энергию фильтрованного сигнала
                sumSquares += (y0 * y0);

                // Сдвигаем память
                x1 = x0;
                y1 = y0;
            }

            if (updateState)
            {
                filterStateX[ch] = x1;
                filterStateY[ch] = y1;
            }
        }

        // Средний квадрат (Mean Square)
        double meanSquare = sumSquares / (double)(numSamples * numCh);

        // Интегрируем во времени (чтобы получить медленный RMS)
        // RMS_avg = old * coeff + new * (1-coeff)
        // Здесь мы сглаживаем ЭНЕРГИЮ (квадраты), а не амплитуду. Это физически верно.
        // ВНИМАНИЕ: Для input и output нужны разные аккумуляторы.

        float& accumulator = updateState ? accumulatorIn : accumulatorOut;
        accumulator = accumulator * rmsCoeff + (float)meanSquare * (1.0f - rmsCoeff);

        return accumulator; // Возвращаем сглаженную энергию
    }

    double fs = 44100.0;
    float hpfCoeff = 0.0f;
    float rmsCoeff = 0.0f;

    // Состояние HPF фильтра (храним историю между блоками -> НЕТ ЩЕЛЧКОВ!)
    std::array<float, 2> filterStateX {0.0f, 0.0f};
    std::array<float, 2> filterStateY {0.0f, 0.0f};

    // Аккумуляторы энергии (RMS memory)
    float accumulatorIn = 0.0f;
    float accumulatorOut = 0.0f;

    float currentInEnergy = 0.0f;
    float currentOutEnergy = 0.0f;

    juce::LinearSmoothedValue<float> smoothedComp;
};
