#pragma once

#include <cmath>
#include <algorithm>
#include "../JuceHeader.h"
#include "../CoheraTypes.h"

class MathSaturator
{
public:
    static constexpr float PHI = 1.61803398875f;
    static constexpr float PI  = 3.14159265359f;
    static constexpr float E   = 2.71828182846f;

    float processSample(float input, float drive, Cohera::SaturationMode mode)
    {
        float x = input * drive;
        float out = 0.0f;

        switch (mode)
        {
            // --- DIVINE ---

            case Cohera::SaturationMode::GoldenRatio:
                // Tanh + Harmonics scaled by Phi
                out = std::tanh(x) + (std::tanh(x*x) * (1.0f/PHI) * 0.15f);
                break;

            case Cohera::SaturationMode::EulerTube:
                // Sigmoid: 2/(1+e^-2x) - 1
                out = (2.0f / (1.0f + std::exp(-2.0f * std::clamp(x, -5.f, 5.f)))) - 1.0f;
                break;

            case Cohera::SaturationMode::PiFold:
                // Sine folding
                out = std::sin(x * (PI / 2.0f));
                break;

            case Cohera::SaturationMode::Fibonacci:
                 // Staircase quantization
                 {
                     float ax = std::abs(x);
                     float s = (x > 0) ? 1.f : -1.f;
                     if (ax > 1.0f) ax = 1.0f + (ax-1.0f)*0.2f; // Soft clip top
                     else if (ax > 0.618f) ax = 0.618f + (ax-0.618f)*0.5f; // Phi compression
                     out = ax * s;
                 }
                 break;

            case Cohera::SaturationMode::SuperEllipse:
                {
                    float n = 2.5f;
                    float s = (x > 0) ? 1.f : -1.f;
                    float ax = std::min(1.0f, std::abs(x));
                    out = s * (1.0f - std::pow(1.0f - std::pow(ax, n), 1.0f/n));
                }
                break;

            // --- CLASSIC ---

            case Cohera::SaturationMode::AnalogTape:
                // ArcTan - классическая эмуляция ленты
                out = (2.0f / PI) * std::atan(x * PI * 0.5f);
                break;

            case Cohera::SaturationMode::VintageConsole:
                // Cubic Clipping (Pentode/Transistor)
                // x - x^3/3
                if (x > 1.5f) out = 1.0f;
                else if (x < -1.5f) out = -1.0f;
                else out = x - (x * x * x) / 3.0f;
                break;

            case Cohera::SaturationMode::DiodeClassA:
                // Asymmetric: x for x<0, exponential for x>0
                if (x > 0) out = (1.0f - std::exp(-x));
                else out = std::tanh(x); // Softer on negative
                break;

            case Cohera::SaturationMode::TubeDriver:
                // Asymmetric Tanh (Bias)
                out = std::tanh(x + 0.2f) - std::tanh(0.2f);
                break;

            case Cohera::SaturationMode::DigitalFuzz:
                // Hard Clipping
                out = std::max(-1.0f, std::min(1.0f, x));
                break;

            case Cohera::SaturationMode::BitDecimator:
                {
                    float steps = 8.0f; // 3 bit equivalent roughly
                    out = std::round(x * steps) / steps;
                }
                break;

            case Cohera::SaturationMode::Rectifier:
                out = std::abs(x); // Full wave rectification
                break;
        }

        return out;
    }
};
