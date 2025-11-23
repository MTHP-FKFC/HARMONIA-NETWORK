#pragma once

#include <cmath>
#include <algorithm>
#include <juce_core/juce_core.h> // Для JUCE_SNAP_TO_ZERO

namespace Cohera {

class ThermalModel
{
public:
    void prepare(double sampleRate)
    {
        // Physics: T = 0.25s (время остывания)
        coolingCoeff = std::exp(-1.0f / (0.25f * (float)sampleRate));
        
        // Physics: Чувствительность нагрева (подстроено под square law)
        heatingFactor = 0.1f; 
        
        temperature = 20.0f; // Комнатная температура (старт)
    }

    void reset()
    {
        temperature = 20.0f;
    }

    // Возвращает температуру для UI (thread-safe for reading float)
    float getCurrentTemp() const { return temperature; }

    // Возвращает DC Bias (смещение рабочей точки)
    float process(float inputSignal)
    {
        // 1. Закон Джоуля-Ленца: P ~ U^2
        // Используем квадрат для физически верного нагрева
        float energy = inputSignal * inputSignal;

        // 2. Термодинамика (Нагрев + Остывание)
        // Temp[n] = Temp[n-1] * cool + Energy * heat
        float currentTemp = temperature;
        currentTemp = (currentTemp * coolingCoeff) + (energy * heatingFactor);

        // 3. Защита от денормалов (CPU Safety)
        // Если очень холодно - обнуляем микро-значения остатка
        JUCE_SNAP_TO_ZERO(currentTemp); 
        
        // Clamp (Лампа не может быть горячее Солнца, условно 150 градусов)
        if (currentTemp > 150.0f) currentTemp = 150.0f;
        // И не холоднее комнаты
        if (currentTemp < 20.0f) currentTemp = 20.0f;

        temperature = currentTemp;

        // 4. Эффект Bias Drift
        // Чем горячее лампа, тем сильнее плывет рабочая точка (Bias).
        // Нормализуем: (Temp - 20) / 100 * maxBias
        float thermalStress = (temperature - 20.0f) * 0.01f;
        
        // Возвращаем небольшое постоянное смещение, которое добавит четных гармоник
        return thermalStress * 0.05f; 
    }

private:
    float coolingCoeff = 0.999f;
    float heatingFactor = 0.05f;
    float temperature = 20.0f; // В градусах Цельсия
};

} // namespace