#pragma once
#include <cmath>
#include <algorithm>
#include "../JuceHeader.h"

class DynamicsRestorer
{
public:
    void prepare(double sampleRate)
    {
        // Очень быстрая атака и релиз для точного отслеживания транзиентов
        // 5-10 мс, чтобы ловить панч, но не создавать искажений амплитудной модуляции (AM)
        float timeMs = 10.0f;
        coeff = std::exp(-1.0f / (timeMs * 0.001f * (float)sampleRate));

        envIn = 0.0f;
        envOut = 0.0f;
    }

    void reset()
    {
        envIn = 0.0f;
        envOut = 0.0f;
    }

    // Возвращает обработанный сэмпл
    // cleanSample: сигнал ДО сатурации
    // satSample: сигнал ПОСЛЕ сатурации
    // amount: 0.0 (без восстановления) .. 1.0 (полное восстановление динамики)
    float process(float cleanSample, float satSample, float amount)
    {
        // 1. Детекция огибающих (Absolute Peak Follower)
        float absIn = std::abs(cleanSample);
        float absOut = std::abs(satSample);

        // Однополюсные фильтры для сглаживания огибающей
        if (absIn > envIn) envIn = absIn;           // Instant Attack
        else               envIn = envIn * coeff + absIn * (1.0f - coeff); // Release

        if (absOut > envOut) envOut = absOut;
        else                 envOut = envOut * coeff + absOut * (1.0f - coeff);

        // 2. Вычисление коррекции
        // Нам нужно привести уровень Out к уровню In.
        // Target = In. Current = Out. Gain = Target / Current.

        float gain = 1.0f;

        // Защита от деления на ноль и шума
        if (envOut > 0.001f)
        {
            gain = envIn / envOut;
        }

        // Ограничиваем гейн, чтобы не было взрывов на тихих хвостах
        // Максимум +12дБ, минимум -24дБ
        gain = juce::jlimit(0.06f, 4.0f, gain);

        // 3. Применение с учетом amount (Dry/Wet для динамики)
        // Если amount = 0, gain = 1.0. Если amount = 1, gain = рассчитанный.
        float finalGain = 1.0f + (gain - 1.0f) * amount;

        return satSample * finalGain;
    }

private:
    float envIn = 0.0f;
    float envOut = 0.0f;
    float coeff = 0.0f;
};
