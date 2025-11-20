#pragma once

#include <cmath>
#include <algorithm>

class TransientDetector
{
public:
    void prepare(double sampleRate)
    {
        // Атака мгновенная, релиз быстрый (20-30мс), чтобы поймать только "щелчок"
        decay = std::exp(-1.0f / (0.03f * (float)sampleRate));
        currentValue = 0.0f;
        lastSample = 0.0f;
    }

    void reset() {
        currentValue = 0.0f;
        lastSample = 0.0f;
    }

    // Возвращает 0.0 ... 1.0 (сила транзиента)
    float process(float input)
    {
        // 1. Вычисляем скорость изменения (Delta)
        // High-pass фильтр по сути
        float delta = std::abs(input - lastSample);
        lastSample = input;

        // 2. Envelope Follower для дельты
        currentValue *= decay;

        // Бустим дельту, чтобы нормальные удары давали 1.0
        float boost = 2.0f;
        if ((delta * boost) > currentValue)
        {
            currentValue = delta * boost;
        }

        return std::min(1.0f, currentValue);
    }

private:
    float currentValue = 0.0f;
    float lastSample = 0.0f;
    float decay = 0.0f;
};
