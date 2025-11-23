#pragma once
#include "../JuceHeader.h"

class PsychoAcousticGain
{
public:
    void prepare(double sampleRate)
    {
        fs = sampleRate;

        // 1. Настройка K-Weighting (приблизительная модель ITU-R BS.1770)
        // Это фильтр, который имитирует восприятие громкости ухом.

        // Stage 1: Pre-filter (High Shelf). Буст ВЧ, так как ухо чувствительнее к ним.
        // +4dB @ 2000Hz
        auto coefsShelf = juce::dsp::IIR::Coefficients<float>::makeHighShelf(fs, 1500.0f, 1.0f, 1.58f); // ~4dB gain
        filterShelfDry.coefficients = coefsShelf;
        filterShelfWet.coefficients = coefsShelf;

        // Stage 2: RLB Filter (High Pass). Срез НЧ, чтобы бас не пампил.
        // Cutoff ~100Hz (стандарт K-weighting)
        auto coefsHP = juce::dsp::IIR::Coefficients<float>::makeHighPass(fs, 100.0f);
        filterHPDry.coefficients = coefsHP;
        filterHPWet.coefficients = coefsHP;

        // Сброс
        reset();

        // Сглаживание самого гейна (инерция стрелки VU-метра)
        // 300-400 мс для естественности
        smoothedGain.reset(fs, 0.4f);
        smoothedGain.setCurrentAndTargetValue(1.0f);
    }

    void reset()
    {
        filterShelfDry.reset(); filterShelfWet.reset();
        filterHPDry.reset();    filterHPWet.reset();

        integratedDry = 0.0f;
        integratedWet = 0.0f;
        lastValidGain = 1.0f; // Reset memory to prevent jumps
        
        // Коэффициент интеграции для RMS (Loudness window ~400ms)
        // Это дает "Momentary Loudness"
        integrationCoeff = 1.0f - std::exp(-1.0f / (0.4f * (float)fs));
    }

    // Основной метод: подаем пару сэмплов (L/R), получаем КУДА крутить гейн
    float processStereoSample(float dryL, float dryR, float wetL, float wetR)
    {
        // 1. Суммируем в моно для анализа (энергия)
        float dryMono = (dryL + dryR) * 0.5f;
        float wetMono = (wetL + wetR) * 0.5f;

        // 2. Применяем K-Weighting (фильтрация восприятия)
        // Сначала Shelf, потом HP
        float dryPerc = filterHPDry.processSample(filterShelfDry.processSample(dryMono));
        float wetPerc = filterHPWet.processSample(filterShelfWet.processSample(wetMono));

        // 3. Считаем Энергию (квадраты)
        float dryPow = dryPerc * dryPerc;
        float wetPow = wetPerc * wetPerc;

        // 4. Интегрируем (сглаживаем измерение)
        integratedDry += (dryPow - integratedDry) * integrationCoeff;
        integratedWet += (wetPow - integratedWet) * integrationCoeff;

        // 5. Вычисляем целевой гейн
        // По умолчанию держим последний валидный гейн (защита от скачков)
        float targetGain = lastValidGain;
        
        // Порог тишины (-60dB approx)
        if (integratedWet > 1e-6f && integratedDry > 1e-6f)
        {
            // Gain = sqrt(DryEnergy / WetEnergy)
            float rawGain = std::sqrt(integratedDry / integratedWet);
            
            // Safety Clamps (-20dB to +12dB)
            targetGain = juce::jlimit(0.1f, 4.0f, rawGain);
            
            // Запоминаем как валидный
            lastValidGain = targetGain;
        }

        // 6. Сглаживаем изменение гейна (чтобы не было "дребезга")
        smoothedGain.setTargetValue(targetGain);
        return smoothedGain.getNextValue();
    }

private:
    double fs = 44100.0;

    // Фильтры для Dry и Wet цепей (нужны отдельные состояния!)
    juce::dsp::IIR::Filter<float> filterShelfDry, filterShelfWet;
    juce::dsp::IIR::Filter<float> filterHPDry, filterHPWet;

    float integratedDry = 0.0f;
    float integratedWet = 0.0f;
    float integrationCoeff = 0.01f;
    
    // Memory to prevent jumps in silence
    float lastValidGain = 1.0f;
    
    juce::LinearSmoothedValue<float> smoothedGain;
};
