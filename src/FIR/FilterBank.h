#pragma once

#include <array>
#include <juce_dsp/juce_dsp.h>
#include "FIR/fir_coeffs_multi_sr.h" // твой текущий хедер с 256-tap
#include "analyzer/AnalyzerPipelineConfig.h" // For MaterialType

enum class FilterPhaseMode
{
    Legacy128,      // текущее поведение (первые 128 отсчётов)
    LinearFIR256,   // честный полный импульс (Phase 2.2)
    MinFIR128       // настоящий min-phase (Phase 2.3)
};

enum class FilterBankRole
{
    Analyzer,   // Анализ (CoheraNetwork)
    Playback    // Плейбек (CoheraProcessor)
};

enum class CrossoverProfile
{
    Default,        // Стандартные кроссоверы (125/800/2500/5000 Hz)
    BassHeavy,      // Усиленные НЧ (80/600/2200/6000 Hz)
    Vocal,          // Вокал-ориентированные (150/900/3000/8000 Hz)
    Bright,         // Яркие миксы (200/1200/4000/10000 Hz)
    Percussive,     // Перкуссия (100/700/2800/7000 Hz)
    Synthetic,      // Синтетика (180/1500/5000/12000 Hz)
    CymbalHeavy,    // Тарелки (250/1600/6000/14000 Hz)
    MixComplex      // Комплексные миксы (adaptive)
};

enum class AnalysisWindowMode
{
    Hann,       // Default - good balance
    Hamming,    // Better stopband attenuation
    Blackman,   // Best stopband attenuation
    Kaiser      // Customizable (future)
};

struct FilterBankConfig
{
    FilterBankRole      role          = FilterBankRole::Playback;
    FilterPhaseMode     phaseMode     = FilterPhaseMode::Legacy128;
    CrossoverProfile    profile       = CrossoverProfile::Default;
    AnalysisWindowMode  analysisWindowMode = AnalysisWindowMode::Hann;  // For analyzer role

    int                 numBands      = 6;      // сейчас 6-полосный
    double              sampleRate    = 44100.0;
    juce::uint32        maxBlockSize  = 2048;
};

// Global function for material-to-profile mapping
CrossoverProfile mapMaterialToProfile(Analyzer::MaterialType materialType, float materialConfidence = 1.0f);

// Класс для управления профилями кроссовера с адаптацией под материал
class CrossoverProfileManager
{
public:
    CrossoverProfileManager();

    // Основная функция: маппинг материала на профиль с гистерезисом
    CrossoverProfile mapMaterialToProfile(Analyzer::MaterialType materialType,
                                        float materialConfidence = 1.0f);

    // Получение частот для профиля
    struct CrossoverFrequencies {
        float lowMid = 125.0f;     // Low-Mid crossover
        float midHigh = 800.0f;    // Mid-High crossover
        float highVeryHigh = 2500.0f; // High-VeryHigh crossover
        float veryHighLimit = 5000.0f; // Very High upper limit
    };

    CrossoverFrequencies getFrequenciesForProfile(CrossoverProfile profile) const;

    // Гистерезис параметры
    void setHysteresisThreshold(float threshold) { hysteresisThreshold = threshold; }
    float getHysteresisThreshold() const { return hysteresisThreshold; }

private:
    // Гистерезис состояние
    CrossoverProfile currentProfile = CrossoverProfile::Default;
    float hysteresisThreshold = 0.7f; // 70% confidence needed to switch

    // Вспомогательные функции
    CrossoverProfile materialToProfileDirect(Analyzer::MaterialType material) const;
    bool shouldSwitchProfile(Analyzer::MaterialType newMaterial,
                           float confidence,
                           CrossoverProfile currentProfile) const;
};

class PlaybackFilterBank
{
public:
    PlaybackFilterBank() = default;

    void prepare (const FilterBankConfig& cfg);
    void reset();

    // Разложить входной буфер на полосы
    void splitIntoBands (const juce::AudioBuffer<float>& input,
                         juce::AudioBuffer<float>* bandBuffers[],
                         int numSamples);

    int getLatencySamples() const noexcept { return latencySamples; }

private:
    FilterBankConfig config;

    int latencySamples = 0;

    // 2 канала × 6 полос (как сейчас)
    std::array<std::array<juce::dsp::FIR::Filter<float>, 6>, 2> firFilters;

    void buildFirFilters(); // перенесём сюда всю логику выбора coeffs

    // Phase 2.3.2: Get frequency indices for crossover profile
    std::array<int, 5> getFrequencyIndicesForProfile(CrossoverProfile profile) const;
};

class AnalyzerFilterBank
{
public:
    AnalyzerFilterBank() = default;

    void prepare (const FilterBankConfig& cfg);
    void reset();

    // Аналогичный API для анализатора
    void splitIntoBands (const juce::AudioBuffer<float>& input,
                         juce::AudioBuffer<float>* bandBuffers[],
                         int numSamples);

    int getLatencySamples() const noexcept { return latencySamples; }

private:
    FilterBankConfig config;
    int latencySamples = 0;

    // Аналогичная структура для анализатора
    std::array<std::array<juce::dsp::FIR::Filter<float>, 6>, 2> firFilters;

    void buildFirFilters();
};
