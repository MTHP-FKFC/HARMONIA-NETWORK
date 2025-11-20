#pragma once

#include <cmath>
#include <algorithm>

enum class SaturationType
{
    Clean,
    WarmTube,   // std::tanh (Soft Clip)
    HardClip,   // Жесткое ограничение
    Rectifier   // Выпрямление (для будущих фич)
};

class Waveshaper
{
public:
    Waveshaper() = default;

    // Обработка одного сэмпла
    // input: входящий сигнал
    // drive: множитель усиления (1.0 = чисто, 10.0 = грязь)
    // type: тип искажения
    float processSample(float input, float drive, SaturationType type)
    {
        // 1. Входное усиление (Drive)
        float x = input * drive;

        // 2. Математика искажения
        switch (type)
        {
            case SaturationType::WarmTube:
                // Классическая ламповая сатурация
                // std::tanh плавно сжимает сигнал к +/- 1.0
                return std::tanh(x);

            case SaturationType::HardClip:
                // Цифровой "кирпич". Жесткий срез.
                return std::clamp(x, -1.0f, 1.0f);

            case SaturationType::Rectifier:
                // Абсолютное значение (удвоение частоты)
                // Пока просто заглушка, реализуем в этапе "Фичи"
                return std::abs(x);

            case SaturationType::Clean:
            default:
                return x;
        }
    }
};
