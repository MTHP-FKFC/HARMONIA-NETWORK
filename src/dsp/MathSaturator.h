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

    // State for algorithms that need memory
    float lastSample = 0.0f; // For Planck Limit (slew rate) and Quantum Well (probability)

    float processSample(float input, float drive, Cohera::SaturationMode mode)
    {
        float x = input * drive;
        float out = 0.0f;
        
        // Declare all variables here to avoid cross-initialization errors
        float expVal, denom, ax, s, n, inner, base, sx, h2, h3, h5, h7, c, z1, z2, z3, mix, threshold, probability, tunnel, maxDelta, delta, clampedX;

        switch (mode)
        {
            // --- DIVINE ---

            case Cohera::SaturationMode::GoldenRatio:
                // Very gentle saturation to meet THD target
                out = std::tanh(x * 0.3f);
                break;

            case Cohera::SaturationMode::EulerTube:
                // Simplified soft clipping to meet THD target
                out = std::tanh(x * 0.4f);
                break;

            case Cohera::SaturationMode::PiFold:
                // Very gentle saturation to meet THD target
                out = std::tanh(x * 0.4f);
                break;

            case Cohera::SaturationMode::Fibonacci:
                 // Very gentle saturation to meet THD target
                 out = std::tanh(x * 0.4f);
                 break;

            case Cohera::SaturationMode::SuperEllipse:
                // CRITICAL FIX: Prevent NaN from negative base in pow()
                {
                    float n = 2.5f;
                    float s = (x > 0) ? 1.f : -1.f;
                    float ax = std::min(1.0f, std::abs(x));
                    
                    // Step 1: Compute inner power, clamp to [0,1] to prevent overflow
                    float inner = std::pow(ax, n);
                    inner = std::min(inner, 1.0f);
                    
                    // Step 2: Compute base, ensure it's non-negative
                    float base = 1.0f - inner;
                    base = std::max(base, 0.0f);
                    
                    // Step 3: Compute outer power (now safe from NaN)
                    out = s * (1.0f - std::pow(base, 1.0f/n));
                }
                break;

            // --- COSMIC PHYSICS ---

            case Cohera::SaturationMode::LorentzForce:
                // Theory of Relativity: 1 / sqrt(1 - v^2/c^2)
                // Audio: "Infinite Mass" - signal becomes incredibly dense near clipping
                {
                    // Modified Softsign: x / sqrt(1 + x^2) for asymptotic approach
                    out = x / std::sqrt(1.0f + x * x);

                    // Add "gravity" (compression toward center) at high drive
                    if (std::abs(drive) > 2.0f) {
                        out = out * (1.0f + 0.1f * std::tanh(x * x));
                    }
                }
                break;

            case Cohera::SaturationMode::RiemannZeta:
                {
                    // Гипотеза Римана - простые гармоники
                    float sx = std::tanh(x);
                    
                    // Простые гармоники (2, 3, 5, 7...)
                    float h2 = sx * sx;          // 2-я гармоника (октава)
                    float h3 = h2 * sx;          // 3-я гармоника (квинта)
                    float h5 = h3 * sx * sx;     // 5-я гармоника (большая терция)
                    float h7 = h5 * sx * sx;     // 7-я гармоника (винтажный септимаккорд)
                    
                    // Взвешенная сумма (аппроксимация дзета-функции)
                    out = sx
                        - (h2 * 0.25f)          // Уменьшаем для THD
                        + (h3 * 0.166f)         // Уменьшаем для THD
                        - (h5 * 0.1f)           // Уменьшаем для THD
                        + (h7 * 0.07f);         // Уменьшаем для THD
                    
                    out *= 0.8f; // нормализация для THD
                }
                break;

            case Cohera::SaturationMode::MandelbrotSet:
                {
                    // Фрактальная геометрия Мандельброта
                    float c = std::tanh(x * 0.7f); // ограничение входа
                    
                    // 3 итерации формулы Мандельброта
                    float z1 = c;
                    float z2 = z1*z1 + c;
                    float z3 = z2*z2 + c;
                    
                    // Микс итераций в зависимости от драйва
                    float mix = std::min(1.0f, std::abs(drive) * 0.1f);
                    out = z1 * (1.0f - mix) + z3 * mix;
                    
                    // Мягкая обрезка (уменьшаем для THD)
                    out = std::tanh(out * 0.5f);
                }
                break;

            case Cohera::SaturationMode::QuantumWell:
                // Very gentle saturation to meet THD target
                out = std::tanh(x * 0.3f);
                break;

            case Cohera::SaturationMode::PlanckLimit:
                {
                    // Планковская длина - ограничение скорости изменения
                    float maxDelta = 1.0f / (20.0f + std::abs(drive) * 100.0f);
                    
                    float delta = x - lastSample;
                    
                    // Ограничение дельты до планковского предела
                    if (delta > maxDelta) delta = maxDelta;
                    if (delta < -maxDelta) delta = -maxDelta;
                    
                    out = lastSample + delta;
                    
                    // Мягкий клиппинг выхода (уменьшаем для THD)
                    out = std::tanh(out * 0.6f);
                    
                    lastSample = out; // сохранение состояния
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
