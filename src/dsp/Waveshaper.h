#pragma once

#include <cmath>
#include <algorithm>

enum class SaturationType
{
    Clean,
    WarmTube,   // Tanh
    HardClip,   // Limit
    Rectifier,  // Ghost Harmonics (Octave Up)
    Crush       // Bitcrush (для будущих фич)
};

class Waveshaper
{
public:
    Waveshaper() = default;

    // Добавляем mix параметр для плавного морфинга
    float processSample(float input, float drive, SaturationType type, float mix = 1.0f)
    {
        float wet = input;
        float x = input * drive;

        switch (type)
        {
            case SaturationType::WarmTube:
                wet = std::tanh(x);
                break;

            case SaturationType::HardClip:
                wet = std::clamp(x, -1.0f, 1.0f);
                break;

            case SaturationType::Rectifier:
                // Эффект октавера/выпрямителя.
                // std::abs делает из синуса "горбы" с удвоенной частотой.
                // Вычитаем 0.5, чтобы убрать DC offset (постоянный ток), который рождает abs.
                wet = (std::abs(x) - 0.5f) * 2.0f;
                break;

            // ... тут добавим Crush позже

            case SaturationType::Clean:
            default:
                return input;
        }

        // Плавный подмес (для Ghost Harmonics нам нужно плавно перетекать)
        return input * (1.0f - mix) + wet * mix;
    }
};
