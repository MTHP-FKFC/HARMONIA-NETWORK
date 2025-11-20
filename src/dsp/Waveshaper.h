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

    // mix: 0.0 = Clean (Input), 1.0 = Full Effect
    float processSample(float input, float drive, SaturationType type, float mix = 1.0f)
    {
        float wet = input;

        // Drive scaling specifically for different types to match loudness
        float x = input * drive;

        switch (type)
        {
            case SaturationType::WarmTube:
                // Классика
                wet = std::tanh(x);
                break;

            case SaturationType::HardClip:
                wet = std::clamp(x, -1.0f, 1.0f);
                break;

            case SaturationType::Rectifier:
                {
                    // NEW ALGORITHM: Foldback Distortion instead of pure Rectifier
                    // Это звучит жирнее и сохраняет больше энергии, но все равно дает октаву.
                    // Суть: если сигнал выходит за 1.0, он "отражается" обратно.

                    float folding = std::abs(x);
                    // Если громко - загибаем волну вниз
                    if (folding > 1.0f) {
                        // fmod дает пилообразный эффект, звучит агрессивно
                        folding = 2.0f - std::fmod(folding, 2.0f);
                        if (folding > 1.0f) folding = 2.0f - folding;
                    }

                    // Восстанавливаем знак (или убираем для октавера)
                    // Для Ghost Harmonics нам нужен именно Absolute (все волны вверх)
                    // Смещаем вниз и умножаем, чтобы вернуть RMS
                    wet = (folding - 0.5f) * 2.5f; // Boost 2.5x чтобы компенсировать потерю низа
                }
                break;

            case SaturationType::Clean:
            default:
                return input;
        }

        // Линейная интерполяция (Crossfade)
        return input + mix * (wet - input);
    }
};
