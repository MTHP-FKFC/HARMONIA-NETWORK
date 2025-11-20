#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_dsp/juce_dsp.h>
#include "dsp/FilterBank.h"
#include "dsp/Waveshaper.h" // Подключаем
#include "dsp/Envelope.h"       // <-- NEW
#include "network/NetworkManager.h" // <-- NEW

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

    // Линия задержки для Dry-сигнала (компенсация FIR-фильтров)
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> dryDelayLine;

    // Плавный микс (Dry/Wet)
    juce::LinearSmoothedValue<float> smoothedMix { 0.0f };

    // Сглаживание параметров (чтобы звук не "хрустел" при вращении ручки)
    juce::SmoothedValue<float> smoothedDrive;
    juce::SmoothedValue<float> smoothedOutput;
    juce::LinearSmoothedValue<float> smoothedNetworkSignal;

    // === NETWORK ===
    EnvelopeFollower envelope; // Измеритель громкости (для Reference)

    // Временные переменные для логики (чтобы не дергать параметры каждый сэмпл)
    int currentGroup = 0;
    bool isReference = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CoheraSaturatorAudioProcessor)
};
