#pragma once

#include <cmath>
#include <algorithm>

class ThermalModel
{
public:
    void prepare(double sampleRate)
    {
        // Лампа остывает медленно (200-300 мс)
        coolingCoeff = std::exp(-1.0f / (0.25f * (float)sampleRate));

        // Нагревается мгновенно от тока
        heatingFactor = 0.05f; // Чувствительность

        temperature = 0.0f;
    }

    void reset()
    {
        temperature = 0.0f;
    }

    // Возвращает смещение (Bias) на основе истории сигнала
    float process(float inputSignal)
    {
        float energy = std::abs(inputSignal);

        // 1. Нагрев
        // Температура растет от энергии сигнала
        temperature += energy * heatingFactor;

        // 2. Охлаждение (потери тепла)
        temperature *= coolingCoeff;

        // Лимит температуры (чтобы лампа не взорвалась)
        if (temperature > 1.0f) temperature = 1.0f;

        // 3. Возвращаем Bias
        // Горячая лампа дает смещение рабочей точки.
        // Это добавляет асимметрию -> четные гармоники (жир).
        return temperature * 0.15f; // Макс смещение 0.15
    }

private:
    float coolingCoeff = 0.0f;
    float heatingFactor = 0.0f;
    float temperature = 0.0f;
};
