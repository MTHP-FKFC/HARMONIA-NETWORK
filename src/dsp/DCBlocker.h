#pragma once

#include <cmath>
#include <algorithm>

class DCBlocker
{
public:
    void reset() {
        x1 = 0.0f;
        y1 = 0.0f;
    }

    // R = 0.995 дает срез около 20-30Hz при 44.1кГц
    // Это убивает DC более эффективно, сохраняя бас
    float process(float input)
    {
        float y = input - x1 + 0.995f * y1;
        x1 = input;
        y1 = y;
        return y;
    }

private:
    float x1 = 0.0f;
    float y1 = 0.0f;
};
