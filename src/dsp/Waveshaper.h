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

            case SaturationType::Rectifier: // Ghost/Foldback
                {
                    // SINE FOLDBACK DISTORTION
                    // Это секрет "рычащих" басов в DnB и Dubstep.
                    // Вместо обрезки, волна "отражается" обратно.
                    // Дает металлический призвук и кучу гармоник.

                    // Умножаем на 1.5, чтобы эффект наступал раньше
                    float arg = x * 1.5f;

                    // std::sin от большого аргумента создает "завороты" волны
                    wet = std::sin(arg);

                    // Добавляем немного оригинального низа, чтобы не терять тело
                    wet = wet * 0.8f + x * 0.2f;
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
