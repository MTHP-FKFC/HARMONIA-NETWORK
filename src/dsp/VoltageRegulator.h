#pragma once

#include <cmath>
#include <algorithm>

class VoltageRegulator
{
public:
    void prepare(double sampleRate)
    {
        // Имитация емкости конденсаторов (инерция питания)
        // Просадка происходит быстро (10ms), восстановление медленно (100ms)
        attackCoeff = std::exp(-1.0f / (0.01f * (float)sampleRate));
        releaseCoeff = std::exp(-1.0f / (0.1f * (float)sampleRate));

        currentSag = 0.0f;
    }

    // globalHeat: 0.0 (тишина) ... 10.0+ (адская нагрузка всей студии)
    // amount: 0.0 ... 1.0 (ручка влияния, например "Console Age")
    float process(float globalHeat, float amount)
    {
        // Целевая просадка.
        // Эмпирически: если Heat=5.0 (5 громких треков), просадка должна быть ощутимой.
        float target = globalHeat * 0.1f;

        // Инерция напряжения (Ballistics)
        if (target > currentSag)
            currentSag = currentSag * attackCoeff + target * (1.0f - attackCoeff);
        else
            currentSag = currentSag * releaseCoeff + target * (1.0f - releaseCoeff);

        // Вычисляем множитель "Голодания" (Starvation Multiplier).
        // 1.0 = Питание в норме.
        // 1.2 = Питание просело -> сигнал клипует раньше (эффективно Drive растет).

        // amount (0..1) регулирует глубину эффекта.
        float starvation = 1.0f + (currentSag * amount * 0.5f);

        // Ограничиваем разумными рамками (макс +50% искажений от просадки)
        return std::min(1.5f, starvation);
    }

private:
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
    float currentSag = 0.0f;
};
