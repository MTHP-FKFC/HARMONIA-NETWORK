#pragma once

#include <cmath>
#include <algorithm>
#include "../JuceHeader.h"
#include "../CoheraTypes.h"

class MathSaturator
{
public:
    static constexpr float PHI = 1.61803398875f;
    static constexpr float PI  = 3.14159265359f;
    static constexpr float E   = 2.71828182846f;

    // State for algorithms that need memory
    float lastSample = 0.0f; // For Planck Limit (slew rate) and Quantum Well (probability)

    float processSample(float input, float drive, Cohera::SaturationMode mode)
    {
        float x = input * drive;
        float out = 0.0f;

        switch (mode)
        {
            // --- DIVINE ---

            case Cohera::SaturationMode::GoldenRatio:
                // Tanh + Harmonics scaled by Phi
                out = std::tanh(x) + (std::tanh(x*x) * (1.0f/PHI) * 0.15f);
                break;

            case Cohera::SaturationMode::EulerTube:
                // Sigmoid: 2/(1+e^-2x) - 1
                out = (2.0f / (1.0f + std::exp(-2.0f * std::clamp(x, -5.f, 5.f)))) - 1.0f;
                break;

            case Cohera::SaturationMode::PiFold:
                // Sine folding
                out = std::sin(x * (PI / 2.0f));
                break;

            case Cohera::SaturationMode::Fibonacci:
                 // Staircase quantization
                 {
                     float ax = std::abs(x);
                     float s = (x > 0) ? 1.f : -1.f;
                     if (ax > 1.0f) ax = 1.0f + (ax-1.0f)*0.2f; // Soft clip top
                     else if (ax > 0.618f) ax = 0.618f + (ax-0.618f)*0.5f; // Phi compression
                     out = ax * s;
                 }
                 break;

            case Cohera::SaturationMode::SuperEllipse:
                {
                    float n = 2.5f;
                    float s = (x > 0) ? 1.f : -1.f;
                    float ax = std::min(1.0f, std::abs(x));
                    out = s * (1.0f - std::pow(1.0f - std::pow(ax, n), 1.0f/n));
                }
                break;

            // --- COSMIC PHYSICS ---

            case Cohera::SaturationMode::LorentzForce:
                // Theory of Relativity: 1 / sqrt(1 - v^2/c^2)
                // Audio: "Infinite Mass" - signal becomes incredibly dense near clipping
                {
                    // Modified Softsign: x / sqrt(1 + x^2) for asymptotic approach
                    out = x / std::sqrt(1.0f + x * x);

                    // Add "gravity" (compression toward center) at high drive
                    if (std::abs(drive) > 2.0f) {
                        out = out * (1.0f + 0.1f * std::tanh(x * x));
                    }
                }
                break;

            case Cohera::SaturationMode::RiemannZeta:
                // Riemann Hypothesis: Prime number harmonics
                // Audio: Harmonics only on prime numbers (2, 3, 5, 7, 11...)
                {
                    // Limit input to prevent series divergence
                    float sx = std::tanh(x);

                    // Prime harmonics (alternating series)
                    float h2 = sx * sx;          // 2nd (Octave)
                    float h3 = h2 * sx;          // 3rd (Fifth)
                    float h5 = h3 * sx * sx;     // 5th (Major Third)
                    float h7 = h5 * sx * sx;     // 7th (Vintage 7th)

                    // Weighted sum (Zeta function approximation)
                    out = sx
                        - (h2 * 0.5f)           // 2nd harmonic
                        + (h3 * 0.333f)         // 3rd harmonic
                        - (h5 * 0.2f)           // 5th harmonic
                        + (h7 * 0.142f);        // 7th harmonic

                    // Normalization
                    out *= 1.2f;
                }
                break;

            case Cohera::SaturationMode::MandelbrotSet:
                // Fractal Geometry: z = z^2 + c
                // Audio: Iterative nonlinearity creating "grit" texture
                {
                    float z = 0.0f;
                    float c = std::tanh(x); // Limit input

                    // 3 iterations of Mandelbrot formula
                    // z₁ = 0² + c = c
                    // z₂ = c² + c
                    // z₃ = (c²+c)² + c

                    float z1 = c;
                    float z2 = z1*z1 + c;
                    float z3 = z2*z2 + c;

                    // Mix iterations based on drive level
                    float mix = std::min(1.0f, std::abs(drive) * 0.2f);
                    out = z1 * (1.0f - mix) + z3 * mix;

                    // Hard clamp (fractals can go to infinity)
                    out = std::max(-1.0f, std::min(1.0f, out));
                }
                break;

            case Cohera::SaturationMode::QuantumWell:
                // Quantum Mechanics: Particle in superposition
                // Audio: Probabilistic saturation with "tunneling"
                {
                    float threshold = 0.8f;
                    float ax = std::abs(x);
                    float s = (x > 0) ? 1.f : -1.f;

                    if (ax > threshold) {
                        // Quantum uncertainty - generate pseudo-random from sample
                        // Using fine structure constant (137) as multiplier
                        float probability = std::abs(std::sin(x * 137.036f));

                        // If "lucky", signal tunnels slightly above threshold
                        float tunnel = (ax - threshold) * probability * 0.5f;
                        out = s * (threshold + tunnel);
                    } else {
                        out = x;
                    }
                }
                break;

            case Cohera::SaturationMode::PlanckLimit:
                // Planck Length: Minimum possible size in universe
                // Audio: Slew rate limiting creates "viscous" sound
                {
                    // Maximum delta depends on drive (higher drive = more viscous)
                    float maxDelta = 1.0f / (10.0f + std::abs(drive) * 50.0f);

                    float delta = x - lastSample;

                    // Clamp delta to Planck limit
                    if (delta > maxDelta) delta = maxDelta;
                    if (delta < -maxDelta) delta = -maxDelta;

                    out = lastSample + delta;

                    // Soft clip output
                    out = std::tanh(out);

                    lastSample = out; // Store state
                }
                break;

            // --- CLASSIC ---

            case Cohera::SaturationMode::AnalogTape:
                // ArcTan - классическая эмуляция ленты
                out = (2.0f / PI) * std::atan(x * PI * 0.5f);
                break;

            case Cohera::SaturationMode::VintageConsole:
                // Cubic Clipping (Pentode/Transistor)
                // x - x^3/3
                if (x > 1.5f) out = 1.0f;
                else if (x < -1.5f) out = -1.0f;
                else out = x - (x * x * x) / 3.0f;
                break;

            case Cohera::SaturationMode::DiodeClassA:
                // Asymmetric: x for x<0, exponential for x>0
                if (x > 0) out = (1.0f - std::exp(-x));
                else out = std::tanh(x); // Softer on negative
                break;

            case Cohera::SaturationMode::TubeDriver:
                // Asymmetric Tanh (Bias)
                out = std::tanh(x + 0.2f) - std::tanh(0.2f);
                break;

            case Cohera::SaturationMode::DigitalFuzz:
                // Hard Clipping
                out = std::max(-1.0f, std::min(1.0f, x));
                break;

            case Cohera::SaturationMode::BitDecimator:
                {
                    float steps = 8.0f; // 3 bit equivalent roughly
                    out = std::round(x * steps) / steps;
                }
                break;

            case Cohera::SaturationMode::Rectifier:
                out = std::abs(x); // Full wave rectification
                break;
        }

        return out;
    }
};
