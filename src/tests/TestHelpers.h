#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>
#include "../dsp/StereoFocus.h"

namespace CoheraTests {

// Генератор синуса
static void fillSine(juce::AudioBuffer<float>& buffer, double sampleRate, float freqHz)
{
    auto numSamples = buffer.getNumSamples();
    auto numChannels = buffer.getNumChannels();
    double phase = 0.0;
    double phaseInc = juce::MathConstants<double>::twoPi * freqHz / sampleRate;

    for (int i = 0; i < numSamples; ++i)
    {
        float sample = (float)std::sin(phase);
        phase += phaseInc;
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.setSample(ch, i, sample);
    }
}

// Генератор импульса (для проверки задержки)
static void fillImpulse(juce::AudioBuffer<float>& buffer, int position = 0)
{
    buffer.clear();
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        buffer.setSample(ch, position, 1.0f);
}

// Проверка: буфер не должен быть пустым (Silence check)
static bool isSilent(const juce::AudioBuffer<float>& buffer)
{
    return buffer.getMagnitude(0, buffer.getNumSamples()) < 0.00001f;
}

// Поиск позиции пика (для проверки Latency)
static int findPeakPosition(const juce::AudioBuffer<float>& buffer)
{
    int peakPos = -1;
    float maxVal = -1.0f;

    const float* data = buffer.getReadPointer(0);
    for(int i=0; i<buffer.getNumSamples(); ++i) {
        if(std::abs(data[i]) > maxVal) {
            maxVal = std::abs(data[i]);
            peakPos = i;
        }
    }
    return peakPos;
}

// Сравнение двух буферов (с допуском epsilon)
static bool areBuffersEqual(const juce::AudioBuffer<float>& a, const juce::AudioBuffer<float>& b, float epsilon = 1e-4f)
{
    if (a.getNumSamples() != b.getNumSamples()) return false;

    for (int ch = 0; ch < a.getNumChannels(); ++ch) {
        for (int i = 0; i < a.getNumSamples(); ++i) {
            if (std::abs(a.getSample(ch, i) - b.getSample(ch, i)) > epsilon)
                return false;
        }
    }
    return true;
}

// Тестирование M/S обработки StereoFocus
static bool testStereoFocus(float focusValue, const juce::AudioBuffer<float>& input,
                           juce::AudioBuffer<float>& output, float expectedMidScale, float expectedSideScale)
{
    if (input.getNumChannels() < 2 || output.getNumChannels() < 2) return false;

    // Clear output buffer
    output.clear();

    StereoFocus focus;
    auto multipliers = focus.getDriveScalars(focusValue * 100.0f);

    // Проверить что multipliers соответствуют ожиданиям (с допуском для makeUp gain)
    if (std::abs(multipliers.midScale - expectedMidScale) > 0.01f ||
        std::abs(multipliers.sideScale - expectedSideScale) > 0.01f) {
        return false;
    }

    // Применить M/S обработку
    for (int i = 0; i < input.getNumSamples(); ++i) {
        float l = input.getSample(0, i);
        float r = input.getSample(1, i);

        // M/S encoding
        float mid = 0.5f * (l + r);
        float side = 0.5f * (l - r);

        // Apply multipliers
        mid *= multipliers.midScale;
        side *= multipliers.sideScale;

        // M/S decoding
        float outL = mid + side;
        float outR = mid - side;

        output.setSample(0, i, outL);
        output.setSample(1, i, outR);
    }

    return true;
}

}
