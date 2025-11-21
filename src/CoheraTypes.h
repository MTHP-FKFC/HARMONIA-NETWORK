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

    // === COSMIC SERIES (Фундаментальная физика) ===
    LorentzForce,   // γ - Плотность (Теория Относительности)
    RiemannZeta,    // ζ - Простые гармоники (Гипотеза Римана)
    MandelbrotSet,  // z² - Текстура (Фрактальная геометрия)
    QuantumWell,    // ψ - Воздух (Квантовая механика)
    PlanckLimit,    // h - Тьма (Планковская длина)

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
    // --- CLASSIC MIXING ---
    Unmasking,      // 0: Duck Volume ("Освободи место")
    Ghost,          // 1: Follow Drive ("Синхронная энергия")
    Gated,          // 2: Reverse Volume ("Играй в паузах")
    StereoBloom,    // 3: Expand Width ("Пространственный взрыв")
    Sympathetic,    // 4: Resonate ("Резонанс")

    // --- ADVANCED MIXING ---
    TransientClone, // 5: Boost Punch ("Заимствование Атаки")
    SpectralSculpt, // 6: Shift Filters ("Динамический Эквалайзер")
    VoltageStarve,  // 7: Sag Voltage ("Энергетический Вампиризм")
    EntropyStorm,   // 8: Chaos Increase ("Управляемый Хаос")
    HarmonicShield  // 9: Reduce Saturation ("Анти-Сатурация")
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