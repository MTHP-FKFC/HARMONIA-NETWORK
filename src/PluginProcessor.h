#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/FilterBank.h"
#include "dsp/Waveshaper.h"
#include "dsp/MathSaturator.h"
#include "dsp/DCBlocker.h"
#include "dsp/Envelope.h"

// Test includes
#include "CoheraTypes.h"
#include "parameters/ParameterSet.h"
#include "parameters/ParameterManager.h"
#include "engine/ProcessingEngine.h"
#include "dsp/DynamicsRestorer.h"
#include "dsp/PsychoAcousticGain.h"
#include "dsp/VoltageRegulator.h"
#include "dsp/ThermalModel.h"
#include "dsp/StereoVariance.h"
#include "dsp/NoiseBreather.h"
#include "dsp/DeltaMonitor.h"
#include "ui/SimpleFFT.h"
#include "dsp/StereoFocus.h"
#include "dsp/HarmonicEntropy.h"
#include "network/NetworkManager.h"

class CoheraSaturatorAudioProcessor : public juce::AudioProcessor
{
public:
    CoheraSaturatorAudioProcessor();
    ~CoheraSaturatorAudioProcessor() override;

    // Стандартные методы JUCE
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    // UI
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Сохранение состояния
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Инфо
    const juce::String getName() const override { return "Cohera Saturator"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    // === GAIN REDUCTION METER ACCESS ===
    // Для визуализации в Editor
    const std::array<float, 6>& getGainReduction() const { return gainReduction; }

    // === VISUALIZATION DATA ACCESS (для UI и тестов) ===
    bool isFFTActive() const {
        return analyzer.isDataReady();
    }

    // FFT данные для SpectrumVisor
    const std::array<float, 512>& getFFTData() const {
        return analyzer.getScopeData();
    }

    // Обработка FFT для GUI (вызывается из timerCallback)
    void processFFTForGUI() {
        analyzer.process();
    }

    // Кол-во программ
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

    // Getter for APVTS
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }

    // Getter for ValueTreeState (for tests)
    juce::AudioProcessorValueTreeState* getValueTreeState() { return &apvts; }

    // Getter for Analyzer
    SimpleFFT& getAnalyzer() { return analyzer; }

    // Public access for Reactor Knob animation
    Cohera::ProcessingEngine& getProcessingEngine() { return processingEngine; }

    // Public access for Visual System
    float getInputRMS() const { return processingEngine.getInputRMS(); }
    float getOutputRMS() const { return outputRMS.load(); }
    float getTransientLevel() const { return lastTransientLevel.load(); }

private:
    std::atomic<float> outputRMS { 0.0f };
    std::atomic<float> lastTransientLevel { 0.0f };

private:
    // === 1. Параметры (State) ===

    // APVTS - единый источник правды для параметров
    juce::AudioProcessorValueTreeState apvts;
    juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // === 2. DSP Модули (Worker bees) ===

    // Мы используем unique_ptr, чтобы контролировать время жизни
    // и не захламлять хедер реализацией (Pimpl-lite подход)

    // Твой золотой фонд
    std::unique_ptr<PlaybackFilterBank> filterBank;

    // Буферы для кроссовера (пока храним тут, позже инкапсулируем в Engine)
    // Используем std::array для жесткого ограничения (6 полос), никакого malloc в аудио-потоке!
    static constexpr int kNumBands = 6;
    std::array<juce::AudioBuffer<float>, kNumBands> bandBuffers;
    std::vector<juce::AudioBuffer<float>*> bandBufferPtrs;

    // === DSP: Divine Math Saturation ===

    // === DC BLOCKERS ===
    // Убивают постоянный ток от асимметричных алгоритмов ([Band][Channel])
    std::array<std::array<DCBlocker, 2>, kNumBands> dcBlockers;

    // === ПРОФЕССИОНАЛЬНЫЙ ГЕЙН-СТЕЙДЖИНГ ===

    // 4x Oversampling (анти-алиасинг для сатурации)
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    static constexpr size_t oversamplingFactor = 4;

    // Линия задержки для Dry-сигнала (компенсация FIR-фильтров + оверсемплинг)
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> dryDelayLine;

    // === EMPHASIS FILTERS (Tone Shaping) ===
    // TPT фильтры для Pre/Post EQ ([0] = Left, [1] = Right)
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> preFilters;  // Tighten (HPF)
    std::array<juce::dsp::StateVariableTPTFilter<float>, 2> postFilters; // Smooth (LPF)

    // Сглаживатели для частот фильтров
    juce::SmoothedValue<float> smoothedTightenFreq;
    juce::SmoothedValue<float> smoothedSmoothFreq;

    // Плавный микс (Dry/Wet)
    juce::LinearSmoothedValue<float> smoothedMix { 0.0f };

    // Сглаживание параметров (чтобы звук не "хрустел" при вращении ручки)
    juce::SmoothedValue<float> smoothedDrive;
    juce::SmoothedValue<float> smoothedOutput;
    juce::LinearSmoothedValue<float> smoothedNetworkSignal;
    juce::SmoothedValue<float> smoothedCompensation;
    juce::SmoothedValue<float> smoothedSatBlend;

    // Восстановители динамики (один на полосу и канал)
    std::array<std::array<DynamicsRestorer, 2>, kNumBands> dynamicsRestorers;

    // Параметр "Dynamics" (насколько мы возвращаем атаку)
    juce::SmoothedValue<float> smoothedDynamics;

    // НОВЫЙ МОДУЛЬ
    PsychoAcousticGain psychoGain;

    // Плавный пуск при инициализации
    juce::LinearSmoothedValue<float> startupFader;

    // === NETWORK ===
    // Вместо одного envelope, теперь массив по полосам
    std::array<EnvelopeFollower, kNumBands> bandEnvelopes;

    // Сглаживатели для приема данных из сети (чтобы модуляция была плавной)
    std::array<juce::LinearSmoothedValue<float>, kNumBands> smoothedNetworkBands;

    // === NETWORK CONTROL (The Holy Trinity) ===
    juce::SmoothedValue<float> smoothedNetDepth;
    juce::SmoothedValue<float> smoothedNetSens;

    // === GLOBAL HEAT ===
    int myInstanceIndex = -1; // ID слота в сети
    juce::LinearSmoothedValue<float> smoothedGlobalHeat;

    // === PUNCH (Transient Control) ===
    juce::SmoothedValue<float> smoothedPunch;

    // === ANALOG MODELING ===
    VoltageRegulator psu; // Блок питания (один на плагин)
    // Тепловая модель (по одной на канал и полосу, т.к. греются они отдельно!)
    // 6 полос * 2 канала = 12 ламп
    std::array<std::array<ThermalModel, 2>, kNumBands> tubes;
    // Параметр аналогового дрейфа
    juce::SmoothedValue<float> smoothedAnalogDrift;

    // === MOJO (Analog Imperfections) ===
    StereoVariance stereoDrift; // Дрейф стерео каналов
    NoiseBreather noiseFloor;   // Дышащий шум
    // Сглаживатели для новых параметров
    juce::SmoothedValue<float> smoothedVariance;
    juce::SmoothedValue<float> smoothedNoise;

    // === PROFESSIONAL TOOLS ===
    DeltaMonitor deltaMonitor;  // Delta monitoring
    StereoFocus stereoFocus;    // M/S focus control
    juce::SmoothedValue<float> smoothedFocus; // Focus parameter smoothing

    // === HARMONIC ENTROPY ===
    // Генераторы хаоса [Band][Channel] - по одному на полосу/канал
    std::array<std::array<HarmonicEntropy, 2>, kNumBands> entropyModules;
    juce::SmoothedValue<float> smoothedEntropy;

    // === GAIN REDUCTION METER ===
    // Для визуализации взаимодействия (Network, Ducking, Ghost)
    std::array<float, kNumBands> gainReduction; // 1.0 = no change, <1.0 = ducked, >1.0 = boosted

    // === SPECTRUM ANALYZER ===
    SimpleFFT analyzer;

    // === QUALITY MODE ===
    // Для экономии CPU в режиме Low Quality
    // isHighQuality удален - был мертвым кодом

    // === NEW ARCHITECTURE ===
    // Новая модульная архитектура
    Cohera::ParameterManager paramManager;
    Cohera::ProcessingEngine processingEngine;

    // Временные переменные для логики (чтобы не дергать параметры каждый сэмпл)
    // currentGroup и isReference удалены - были мертвым кодом

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CoheraSaturatorAudioProcessor)
};
