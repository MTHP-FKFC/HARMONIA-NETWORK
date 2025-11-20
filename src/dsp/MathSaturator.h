#pragma once

#include <cmath>
#include <algorithm>
#include "../JuceHeader.h"

enum class MathMode
{
    GoldenRatio, // Phi
    EulerTube,   // e
    PiFold,      // Pi
    Fibonacci,   // Fractal
    SuperEllipse // Lamé
};

class MathSaturator
{
public:
    MathSaturator() = default;

    // Основные константы
    static constexpr float PHI = 1.61803398875f; // Золотое сечение
    static constexpr float PI  = 3.14159265359f;
    static constexpr float E   = 2.71828182846f;

    float processSample(float input, float drive, MathMode mode)
    {
        float x = input * drive;
        float out = 0.0f;

        switch (mode)
        {
            case MathMode::GoldenRatio:
                // Полиномы Чебышева, взвешенные по Золотому Сечению.
                // T2(x) = 2x^2 - 1, T3(x) = 4x^3 - 3x
                // Смесь: x + (x^2)/Phi + (x^3)/Phi^2...
                {
                    // Мягкий клиппер на входе, чтобы полиномы не взрывались
                    float satX = std::tanh(x);

                    float t1 = satX;
                    float t2 = 2.0f * satX * satX - 1.0f;
                    float t3 = 4.0f * satX * satX * satX - 3.0f * satX;

                    // Смешиваем гармоники по убывающей золотой спирали
                    // Это дает "божественный" спектральный спад
                    out = t1 + (t2 * (1.0f / PHI) * 0.2f) + (t3 * (1.0f / (PHI * PHI)) * 0.1f);

                    // Компенсация DC Offset от T2 (четная гармоника)
                    // T2(-1..1) дает смещение, убираем его
                    out -= (1.0f / PHI) * 0.2f * 0.5f; // Примерная коррекция
                }
                break;

            case MathMode::EulerTube:
                // Сигмоида на базе экспоненты (Logistic Function / Sigmoid)
                // f(x) = 2 / (1 + e^(-2x)) - 1
                // Это звучит гораздо "толще" и "теплее", чем tanh
                {
                    // Защита от переполнения exp
                    float safeX = std::clamp(x, -10.0f, 10.0f);
                    out = (2.0f / (1.0f + std::exp(-2.0f * safeX))) - 1.0f;
                }
                break;

            case MathMode::PiFold:
                // Тригонометрический фолдбэк
                // sin(x * PI / 2) - это мягкий клиппер.
                // Если увеличить аргумент, он начинает "заворачиваться" (fold).
                {
                    // Мягкий старт, потом заворот
                    if (std::abs(x) < 1.0f) {
                        out = std::sin(x * (PI / 2.0f));
                    } else {
                        // За пределами 1.0 начинается магия
                        // Сглаживаем углы через косинус
                        float overflow = x - (x > 0 ? 1.0f : -1.0f);
                        float fold = std::cos(overflow * PI);
                        out = (x > 0 ? 1.0f : -1.0f) * fold;
                    }
                    // Mix с исходником, чтобы не терять атаку
                    out = out * 0.8f + std::tanh(x) * 0.2f;
                }
                break;

            case MathMode::Fibonacci:
                // Фрактальное квантование/клиппинг
                // Режет сигнал на уровнях 1/8, 1/5, 1/3, 1/2 (обратные Фибоначчи)
                {
                    float sign = (x > 0) ? 1.0f : -1.0f;
                    float ax = std::abs(x);

                    // "Ступеньки" Фибоначчи
                    float f1 = 1.0f/1.0f; // 1.0
                    float f2 = 1.0f/2.0f; // 0.5
                    float f3 = 1.0f/3.0f; // 0.333
                    float f4 = 1.0f/5.0f; // 0.2

                    if (ax > f1) ax = f1 + std::tanh(ax - f1) * 0.1f; // Soft ceiling
                    else if (ax > f2) ax = ax + (ax - f2) * 0.5f; // Expansion
                    else if (ax > f3) {} // Linear (no change)
                    else if (ax > f4) ax = ax - (ax - f4) * 0.2f; // Compression

                    out = ax * sign;
                }
                break;

            case MathMode::SuperEllipse:
                // Формула Ламе: (x^n + y^n = 1) -> y = (1 - x^n)^(1/n)
                // Идеальный переход от синуса к квадрату без алиасинга углов.
                // n зависит от драйва
                {
                    float n = 2.0f + std::abs(drive) * 0.5f; // Порядок кривой
                    float sign = (x > 0) ? 1.0f : -1.0f;
                    float ax = std::min(1.0f, std::abs(x)); // Input должен быть <= 1 для этой формулы

                    // Применяем формулу супер-эллипса для шейпинга
                    // Это делает верхушку волны плоской, как стол, но с гладкими краями
                    float curve = 1.0f - std::pow(1.0f - std::pow(ax, n), 1.0f/n);

                    // Если драйв > 1, мы уже клипуем, поэтому миксуем
                    if (std::abs(x) > 1.0f) out = sign; // Hard limit за пределами
                    else out = curve * sign;
                }
                break;
        }

        return out;
    }
};
