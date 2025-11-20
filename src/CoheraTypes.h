#pragma once

#include <vector>
#include <cmath>

namespace Cohera {

// --- Enums ---

enum class SaturationMode {
    // === DIVINE SERIES (Математическая красота) ===
    GoldenRatio,    // φ (Phi) - Гармоничный
    EulerTube,      // e - Теплый, органичный
    PiFold,         // π - Металлический фолдбэк
    Fibonacci,      // Фрактальный грит
    SuperEllipse,   // Идеальный панч

    // === CLASSIC SERIES (Студийная классика) ===
    AnalogTape,     // Лента (Hysteresis emulation via ATAN)
    VintageConsole, // Транзистор (Cubic distortion)
    DiodeClassA,    // Резкий срез (Asymmetric Exp)
    TubeDriver,     // Гитарный перегруз (Asymmetric Tanh)
    DigitalFuzz,    // Жесткая цифра
    BitDecimator,   // Понижение битности
    Rectifier       // Октавер/Глитч
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