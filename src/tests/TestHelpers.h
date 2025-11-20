#pragma once

#include <juce_dsp/juce_dsp.h>
#include <juce_core/juce_core.h>

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

}
