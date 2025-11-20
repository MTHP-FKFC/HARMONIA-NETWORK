#pragma once

class MSMatrix
{
public:
    // Кодируем L/R -> Mid/Side
    // Mid = (L + R) / 2 (сумма)
    // Side = (L - R) / 2 (разница)
    static void encode(float& l, float& r, float& m, float& s)
    {
        m = 0.5f * (l + r);
        s = 0.5f * (l - r);
    }

    // Декодируем Mid/Side -> L/R
    // L = Mid + Side
    // R = Mid - Side
    static void decode(float m, float s, float& l, float& r)
    {
        l = m + s;
        r = m - s;
    }
};
