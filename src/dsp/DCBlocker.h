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

    // R = 0.98 дает срез около 3-5Hz при 44.1кГц
    // Еще более агрессивная DC фильтрация для очень проблемных случаев
    float process(float input)
    {
        float y = input - x1 + 0.98f * y1;
        x1 = input;
        y1 = y;
        return y;
    }

private:
    float x1 = 0.0f;
    float y1 = 0.0f;
};
