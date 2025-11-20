#pragma once

#include <vector>
#include <cmath>

namespace Cohera {

// --- Enums ---

enum class MathMode {
    GoldenRatio, // 0
    EulerTube,   // 1
    PiFold,      // 2
    Fibonacci,   // 3
    SuperEllipse // 4
};

enum class NetworkMode {
    Unmasking,   // 0 Duck
    Ghost,       // 1 Follow
    Gated,       // 2 Reverse
    StereoBloom, // 3
    Sympathetic  // 4
};

enum class QualityMode {
    Eco, // 0
    Pro  // 1
};

enum class NetworkRole {
    Listener,  // 0
    Reference  // 1
};

// --- Constants ---

static constexpr int kNumBands = 6;

} // namespace Cohera