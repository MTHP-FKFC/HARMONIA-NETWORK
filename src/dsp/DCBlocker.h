#pragma once

#include <cmath>
#include <algorithm>
#include <juce_core/juce_core.h>

class DCBlocker
{
public:
    /**
     * @brief Configure the filter for a specific sample rate.
     *
     * Keeps cutoff near 5 Hz across all sample rates to avoid bass loss.
     */
    void prepare(double sampleRate)
    {
        const double targetCutoffHz = 5.0;
        const double twoPi = 2.0 * juce::MathConstants<double>::pi;
        auto newR = 1.0 - (twoPi * targetCutoffHz / std::max(1.0, sampleRate));
        R = juce::jlimit(0.90, 0.999999, newR);
        reset();
    }

    void reset() {
        x1 = 0.0f;
        y1 = 0.0f;
    }

    float process(float input)
    {
        float y = input - x1 + static_cast<float>(R) * y1;
        x1 = input;
        y1 = y;
        return y;
    }

private:
    double R = 0.995;
    float x1 = 0.0f;
    float y1 = 0.0f;
};
