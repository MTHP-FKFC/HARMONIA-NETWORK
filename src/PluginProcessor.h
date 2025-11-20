#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/FilterBank.h"
#include "dsp/Waveshaper.h"
#include "dsp/Envelope.h"
#include "dsp/DynamicsRestorer.h"
#include "dsp/PsychoAcousticGain.h"
#include "dsp/TransientDetector.h"
#include "dsp/VoltageRegulator.h"
#include "dsp/ThermalModel.h"
#include "dsp/StereoVariance.h"
#include "dsp/NoiseBreather.h"
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

    // Кол-во программ
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return {}; }
    void changeProgramName(int, const juce::String&) override {}

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

    // === DSP: Saturation ===
    // Один шейпер на каждую полосу (если захотим хранить состояние, например DC-фильтр)
    std::array<Waveshaper, kNumBands> shapers;

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
    float netSmoothState = 0.0f; // Состояние One-Pole фильтра для Smooth

    // === GLOBAL HEAT ===
    int myInstanceIndex = -1; // ID слота в сети
    juce::LinearSmoothedValue<float> smoothedGlobalHeat;

    // === PUNCH (Transient Control) ===
    // Детекторы транзиентов ([0]=L, [1]=R, работают на высокой частоте)
    std::array<TransientDetector, 2> transDetectors;
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

    // Временные переменные для логики (чтобы не дергать параметры каждый сэмпл)
    int currentGroup = 0;
    bool isReference = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CoheraSaturatorAudioProcessor)
};
