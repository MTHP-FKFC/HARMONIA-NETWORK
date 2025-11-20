#pragma once

#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>

namespace CoheraTests {

class AudioGenerator
{
public:
    // 1. Синтетическая Бочка (Kick Drum)
    // Свип частоты 150Гц -> 40Гц + быстрая атака.
    // Идеально для теста Low-end и TransientEngine.
    static void fillSyntheticKick(juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        buffer.clear();
        auto* left = buffer.getWritePointer(0);
        auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr;

        double phase = 0.0;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            double t = (double)i / sampleRate;

            // Frequency Envelope (Pitch drop)
            double freq = 50.0 + 150.0 * std::exp(-t * 20.0);

            // Amplitude Envelope (Short decay)
            double amp = std::exp(-t * 8.0);

            double phaseInc = juce::MathConstants<double>::twoPi * freq / sampleRate;
            phase += phaseInc;

            float sample = (float)(std::sin(phase) * amp);

            // Немного клиппинга для реализма
            sample = std::tanh(sample * 1.5f);

            left[i] = sample;
            if (right) right[i] = sample;
        }
    }

    // 2. Синтетический Бас (Sawtooth + LPF)
    // Богатый гармониками сигнал.
    // Идеально для теста сатурации и алиасинга.
    static void fillSyntheticBass(juce::AudioBuffer<float>& buffer, double sampleRate)
    {
        buffer.clear();
        auto* l = buffer.getWritePointer(0);
        double phase = 0.0;
        double freq = 60.0; // Sub bass note

        // Simple 1-pole LPF state
        double smoothX = 0.0;

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            // Sawtooth
            float raw = (float)(2.0 * (phase / juce::MathConstants<double>::twoPi) - 1.0);
            phase += juce::MathConstants<double>::twoPi * freq / sampleRate;
            if (phase >= juce::MathConstants<double>::twoPi) phase -= juce::MathConstants<double>::twoPi;

            // Low Pass Filter (cutoff ~200Hz) to simulate amp
            smoothX += (raw - smoothX) * 0.1;

            l[i] = (float)smoothX;
            if (buffer.getNumChannels() > 1) buffer.setSample(1, i, (float)smoothX);
        }
    }

    // 3. Взрыв Шума (Noise Burst / Snare)
    // Для проверки высоких частот и "Smooth" фильтра.
    static void fillNoiseBurst(juce::AudioBuffer<float>& buffer)
    {
        juce::Random rng;
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                // White noise * Envelope
                float env = std::exp(-(float)i * 0.005f); // Fast decay
                data[i] = (rng.nextFloat() * 2.0f - 1.0f) * env;
            }
        }
    }
};

} // namespace CoheraTests
