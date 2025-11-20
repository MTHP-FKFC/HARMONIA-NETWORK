#pragma once

#include <cmath>
#include <random>

class HarmonicEntropy
{
public:
    HarmonicEntropy() : rng(std::random_device{}()) {}

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

    // amount: 0.0 ... 1.0 (сила энтропии)
    // Возвращает значение смещения (DC Offset), которое нужно добавить к сигналу
    float process(float amount)
    {
        if (amount < 0.001f) return 0.0f;

        // Обновляем цель случайным образом каждые ~5-10 мс (чтобы создать шум)
        if (++stepsSinceLastUpdate > updateInterval)
        {
            // Генерируем случайное число от -1 до 1
            std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
            float noise = dist(rng);

            // Random Walk: новая цель зависит от предыдущей (чтобы не скакало резко)
            targetDrift = targetDrift * 0.5f + noise * 0.5f;

            // Случайный интервал обновления для уменьшения периодичности
            updateInterval = 200 + (int)(dist(rng) * 100); // ~200-300 сэмплов
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

    std::mt19937 rng;
};
