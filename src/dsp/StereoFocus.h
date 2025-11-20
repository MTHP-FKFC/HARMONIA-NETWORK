#pragma once

#include <algorithm>
#include <cmath>

class StereoFocus
{
public:
    struct Multipliers
    {
        float midScale;  // Множитель драйва для Mid
        float sideScale; // Множитель драйва для Side
    };

    // focusParam: -100.0 (Mid) ... 0.0 (Even) ... +100.0 (Side)
    Multipliers getDriveScalars(float focusParam)
    {
        // Нормализуем -100..100 в -1..1
        float f = focusParam / 100.0f;

        float m = 1.0f;
        float s = 1.0f;

        if (f < 0.0f)
        {
            // Акцент на MID (левая часть ручки)
            // Side убавляется
            // При f = -1.0 -> Side = 0.0 (Сатурация только в центре)
            s = 1.0f + f; // 1 + (-1) = 0
        }
        else if (f > 0.0f)
        {
            // Акцент на SIDE (правая часть ручки)
            // Mid убавляется
            // При f = 1.0 -> Mid = 0.0 (Сатурация только по бокам)
            m = 1.0f - f;
        }

        // Чуть бустим активный канал, чтобы компенсировать потерю энергии
        // (опционально, но звучит музыкальнее)
        if (f != 0.0f) {
            float makeUp = 1.0f + std::abs(f) * 0.5f;
            if (m > 0.5f) m *= makeUp;
            if (s > 0.5f) s *= makeUp;
        }

        return { std::max(0.0f, m), std::max(0.0f, s) };
    }

    // M/S Матрица (встроенная, чтобы не тащить лишние зависимости)
    static void encode(float l, float r, float& m, float& s)
    {
        m = 0.5f * (l + r);
        s = 0.5f * (l - r);
    }

    static void decode(float m, float s, float& l, float& r)
    {
        l = m + s;
        r = m - s;
    }
};
