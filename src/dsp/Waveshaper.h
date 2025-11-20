#pragma once
#include <cmath>
#include <algorithm>
#include <juce_core/juce_core.h>

enum class SaturationType
{
    Clean,       // Прозрачный
    WarmTube,    // Tanh
    Asymmetric,  // Even Harmonics
    Rectifier,   // Sine Fold (Ghost)
    BitCrush,    // Digital
    HardClip     // Brickwall
};

class Waveshaper
{
public:
    // Чистая функция: Вход + Драйв + Тип -> Выход
    static float process(float input, float drive, SaturationType type)
    {
        float x = input * drive;

        switch (type)
        {
            case SaturationType::Clean:
                return input; // Драйв не влияет на чистоту

            case SaturationType::WarmTube:
                return std::tanh(x);

            case SaturationType::Asymmetric:
                {
                    // Смещенный tanh для четных гармоник
                    float bias = 0.2f;
                    return (std::tanh(x + bias) - std::tanh(bias)) * 1.1f;
                }

            case SaturationType::HardClip:
                return juce::jlimit(-1.0f, 1.0f, x);

            case SaturationType::Rectifier:
                // Sine Foldback (Ghost)
                // Добавляем гейн, так как foldback съедает энергию
                return (std::sin(x * 1.5f) * 0.8f + input * 0.2f) * 1.2f;

            case SaturationType::BitCrush:
                {
                    // Эмуляция биткраша
                    float depth = 16.0f / std::max(1.0f, drive);
                    float crushed = std::round(input * depth) / depth;
                    // Жесткий клип на выходе биткраша
                    return juce::jlimit(-1.0f, 1.0f, crushed);
                }
        }
        return input;
    }
};
