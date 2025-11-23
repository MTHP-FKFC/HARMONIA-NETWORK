#pragma once

#include <cmath>
#include <algorithm>

class EnvelopeFollower
{
public:
    void reset(double sampleRate)
    {
        // Агрессивный сброс.
        // Коэффициент умножения на каждый блок.
        // Чем меньше число, тем быстрее падает уровень.
        // 0.90 означает потерю 10% громкости на каждом шаге, если нет нового пика.
        decayFactor = 0.90f;
        currentValue = 0.0f;
    }

    float process(float input)
    {
        float absIn = std::abs(input);

        // Логика Peak Hold с распадом:
        // 1. Сначала уменьшаем текущее значение (Decay)
        currentValue *= decayFactor;

        // 2. Если новый вход громче - мгновенно прыгаем вверх (Attack = 0ms)
        if (absIn > currentValue)
        {
            currentValue = absIn;
        }

        // Защита от денормалов (очень малых чисел)
        if (currentValue < 0.0001f) currentValue = 0.0f;

        return currentValue;
    }

    float getCurrentValue() const { return currentValue; }

private:
    float currentValue = 0.0f;
    float decayFactor = 0.9f;
};
