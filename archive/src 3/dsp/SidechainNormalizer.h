#pragma once

#include <cmath>
#include <algorithm>
#include <juce_core/juce_core.h>

class SidechainNormalizer
{
public:
    void prepare(double sampleRate)
    {
        // Забываем пик за 3 секунды
        peakDecayCoeff = std::exp(-1.0f / (3.0f * (float)sampleRate));
        currentPeak = 0.0f;
    }

    // Возвращает нормализованный управляющий сигнал (0.0 - 1.0)
    float process(float rawInput)
    {
        // 1. Обновляем бегущий максимум
        currentPeak *= peakDecayCoeff;

        if (rawInput > currentPeak)
            currentPeak = rawInput;

        // Защита от деления на ноль (Noise Floor -60dB)
        float safePeak = std::max(currentPeak, 0.001f);

        // 2. Нормализуем
        float normalized = rawInput / safePeak;

        // 3. Применяем Relative Gate (отсекаем хвосты ниже 30% от пика)
        if (normalized < 0.3f)
            return 0.0f;

        // 4. Растягиваем диапазон (0.3..1.0 -> 0.0..1.0) и делаем кривую
        float result = (normalized - 0.3f) / 0.7f;
        return result * result; // Квадратичная атака для панча
    }

private:
    float currentPeak = 0.0f;
    float peakDecayCoeff = 0.0f;
};
