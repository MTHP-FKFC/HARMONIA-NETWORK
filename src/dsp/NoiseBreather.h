#pragma once

#include <random>

class NoiseBreather
{
public:
    void prepare(double sampleRate)
    {
        // Фильтр для "розового" оттенка шума (High Cut)
        // Чтобы не резал уши цифрой, срежем всё выше 10кГц
        lpf.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass(sampleRate, 8000.0f);
        lpf.reset();

        // High Pass, чтобы убрать низкочастотный гул (Rumble)
        hpf.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass(sampleRate, 300.0f);
        hpf.reset();

        // Энвелоп для дакинга шума (медленный, "дышащий")
        envelope = 0.0f;
        // Attack (шум прячется быстро): 10ms
        attackCoeff = std::exp(-1.0f / (0.01f * (float)sampleRate));
        // Release (шум всплывает медленно): 500ms
        releaseCoeff = std::exp(-1.0f / (0.5f * (float)sampleRate));
    }

    // signalLevel: текущая громкость музыки (для сайдчейна)
    // amount: громкость шума (0..1)
    float getNoiseSample(float signalLevel, float amount)
    {
        if (amount <= 0.001f) return 0.0f;

        // 1. Генерируем Белый Шум
        float white = (random.nextFloat() * 2.0f) - 1.0f;

        // 2. Красим шум (Bandpass: убираем гул и песок)
        float colored = hpf.processSample(lpf.processSample(white));

        // 3. Логика Дакинга (Ducking)
        // Следим за громкостью музыки
        if (signalLevel > envelope)
            envelope = signalLevel; // Мгновенная реакция (шум прячется)
        else
            envelope = envelope * releaseCoeff + signalLevel * (1.0f - releaseCoeff); // Плавное всплытие

        // Инвертируем энвелоп:
        // Если музыка громкая (env=1), гейн шума = 0.
        // Если музыка тихая (env=0), гейн шума = 1.
        // Добавляем "пол" (floor), чтобы шум не исчезал совсем, если amount большой.
        float duckingGain = 1.0f - std::min(1.0f, envelope * 4.0f); // *4 для агрессивного дакинга
        if (duckingGain < 0.0f) duckingGain = 0.0f;

        // 4. Итоговый уровень
        // Базовый уровень шума очень тихий (-50dB..-80dB)
        // amount 1.0 = -40dB (слышно отчетливо)
        float level = amount * 0.01f;

        return colored * duckingGain * level;
    }

private:
    juce::Random random;
    juce::dsp::IIR::Filter<float> lpf;
    juce::dsp::IIR::Filter<float> hpf;

    float envelope = 0.0f;
    float attackCoeff = 0.0f;
    float releaseCoeff = 0.0f;
};
