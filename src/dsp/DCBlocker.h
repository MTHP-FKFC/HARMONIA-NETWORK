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

    // R = 0.999 дает срез около 5-10Hz при 44.1кГц
    // Это убивает DC, но сохраняет самый низкий бас
    float process(float input)
    {
        float y = input - x1 + 0.999f * y1;
        x1 = input;
        y1 = y;
        return y;
    }

private:
    float x1 = 0.0f;
    float y1 = 0.0f;
};
