#pragma once

#include <cmath>
#include <cstdint>

namespace Cohera {

class HarmonicEntropy
{
public:
    HarmonicEntropy() {
        // Seed с уникальным значением при старте
        state = 0xCAFEBABE;
    }

    void prepare(double sampleRate)
    {
        fs = sampleRate;
        // Фильтр для сглаживания шума (делаем его низкочастотным, как "гуляние" напряжения)
        // ~10 Hz - достаточно быстро, чтобы давать текстуру, но не гудеть
        smoothingCoeff = std::exp(-1.0f / (0.02f * (float)fs)); // 20ms

        currentDrift = 0.0f;
        targetDrift = 0.0f;
        stepsSinceLastUpdate = 0;
    }

    void reset()
    {
        currentDrift = 0.0f;
        targetDrift = 0.0f;
    }
    
    // Быстрый ГСЧ (Xorshift32) - lock-free, constant-time
    // Возвращает float -1.0 ... 1.0
    float nextRandom()
    {
        uint32_t x = state;
        x ^= x << 13;
        x ^= x >> 17;
        x ^= x << 5;
        state = x;
        
        // Маппинг uint32 в float 0..1
        // (x * (1.0 / 4294967296.0))
        float f = (float)x * 2.3283064365386963e-10f; 
        return f * 2.0f - 1.0f; // -1..1
    }

    // amount: 0.0 ... 1.0 (сила энтропии)
    // Возвращает значение смещения (DC Offset), которое нужно добавить к сигналу
    float process(float amount)
    {
        if (amount < 0.001f) return 0.0f;

        // Обновляем цель случайным образом каждые ~5-10 мс (чтобы создать шум)
        if (++stepsSinceLastUpdate > updateInterval)
        {
            float noise = nextRandom();

            // Random Walk: новая цель зависит от предыдущей (чтобы не скакало резко)
            targetDrift = targetDrift * 0.5f + noise * 0.5f;

            // Случайный интервал обновления для уменьшения периодичности
            // Используем битовые маски для скорости вместо деления/умножения
            updateInterval = 200 + (state & 127); // 200..327 samples
            stepsSinceLastUpdate = 0;
        }

        // Плавное движение к цели (LPF)
        currentDrift = currentDrift * smoothingCoeff + targetDrift * (1.0f - smoothingCoeff);

        // Масштабируем результат
        // Максимальное смещение 0.15 достаточно, чтобы создать сильные четные гармоники
        return currentDrift * 0.15f * amount;
    }

private:
    double fs = 44100.0;
    float smoothingCoeff = 0.0f;

    float currentDrift = 0.0f;
    float targetDrift = 0.0f;

    int stepsSinceLastUpdate = 0;
    int updateInterval = 256;

    uint32_t state = 123456789; // Xorshift32 state
};

} // namespace Cohera
