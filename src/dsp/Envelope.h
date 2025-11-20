#pragma once

#include <cmath>
#include <algorithm>

class EnvelopeFollower
{
public:
    void reset(double sampleRate)
    {
        // Настраиваем время релиза (спада).
        // 50 мс - очень быстро для резкой реакции на удар бочки
        releaseCoeff = std::exp(-1.0f / (0.05f * (float)sampleRate));
        currentValue = 0.0f;
    }

    float process(float input)
    {
        float absIn = std::abs(input);

        // Instant Attack (Мгновенная атака)
        // Если входящий сигнал громче текущего - прыгаем вверх сразу.
        // Exponential Release (Плавный спад)
        // Если тише - плавно опускаемся.

        if (absIn > currentValue)
            currentValue = absIn;
        else
            currentValue = currentValue * releaseCoeff + absIn * (1.0f - releaseCoeff);

        return currentValue;
    }

    float getCurrentValue() const { return currentValue; }

private:
    float currentValue = 0.0f;
    float releaseCoeff = 0.0f;
};
