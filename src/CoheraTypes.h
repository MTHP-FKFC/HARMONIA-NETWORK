#pragma once

#include <vector>

namespace SampleRateSupport {

// Поддерживаемые частоты дискретизации
const std::vector<double> kSupportedSampleRates = {
    44100.0, 48000.0, 88200.0, 96000.0
};

// Проверяет, поддерживается ли указанная частота дискретизации
inline bool isSupportedSampleRate(double sampleRate) {
    for (double rate : kSupportedSampleRates) {
        if (std::abs(rate - sampleRate) < 1.0) {
            return true;
        }
    }
    return false;
}

// Возвращает ближайшую поддерживаемую частоту дискретизации
inline double getNearestSupportedSampleRate(double sampleRate) {
    double nearest = kSupportedSampleRates[0];
    double minDiff = std::abs(sampleRate - nearest);

    for (double rate : kSupportedSampleRates) {
        double diff = std::abs(sampleRate - rate);
        if (diff < minDiff) {
            minDiff = diff;
            nearest = rate;
        }
    }

    return nearest;
}

} // namespace SampleRateSupport
