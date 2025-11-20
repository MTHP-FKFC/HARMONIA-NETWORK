#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"

// Наши компоненты
#include "FilterBankEngine.h"
#include "MixEngine.h"
#include "../network/NetworkController.h"

namespace Cohera {

class ProcessingEngine
{
public:
    ProcessingEngine()
    {
        // Инициализируем Oversampler (4x, Linear Phase)
        // 2^2 = 4x
        oversampler = std::make_unique<juce::dsp::Oversampling<float>>(2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
    }

    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        sampleRate = spec.sampleRate;
        blockSize = spec.maximumBlockSize;

        // 1. Oversampler init
        oversampler->initProcessing(blockSize);

        // Вычисляем параметры для High Rate (4x)
        double highSampleRate = sampleRate * 4.0;
        int highBlockSize = (int)blockSize * 4;

        juce::dsp::ProcessSpec highSpec { highSampleRate, (juce::uint32)highBlockSize, spec.numChannels };

        // 2. Готовим компоненты
        filterBankEngine.prepare(highSpec); // Работает на 4x

        mixEngine.prepare(spec);            // Работает на 1x (Output)
        networkController.prepare(sampleRate); // Работает на 1x (Analysis)

        updateLatency();
    }

    void reset()
    {
        oversampler->reset();
        filterBankEngine.reset();
        mixEngine.reset();
        networkController.reset();
    }

    // Главный метод, вызываемый из PluginProcessor
    void processBlock(juce::AudioBuffer<float>& buffer, const ParameterSet& params)
    {
        // 1. Network Analysis (на исходной частоте)
        // Получаем модуляцию (от сети или от входа)
        auto netModulations = networkController.process(buffer, params);

        // 2. Oversampling UP (Upsample -> High Rate Block)
        juce::dsp::AudioBlock<float> inputBlock(buffer);
        auto highRateBlock = oversampler->processSamplesUp(inputBlock);

        // 3. Core Processing (FilterBank -> Bands -> Sum)
        // Все происходит на высокой частоте (176k / 192k)
        filterBankEngine.process(highRateBlock, params, netModulations);

        // 4. Oversampling DOWN (Downsample -> Mix Ready)
        oversampler->processSamplesDown(inputBlock);
        // Теперь inputBlock содержит обработанный Wet сигнал (но сэмплрейт вернулся в норму)

        // 5. Mix & Output
        // buffer сейчас содержит WET сигнал (результат оверсемплинга пишет обратно в buffer)
        // Но MixEngine нужен "чистый" вход для Dry.
        // А мы только что перезаписали buffer результатом `processSamplesDown`.

        // ОШИБКА ЛОГИКИ В JUCE OVERSAMPLING:
        // processSamplesDown перезаписывает входной блок.
        // Нам нужно было сохранить копию DRY сигнала ДО оверсемплинга.

        // Исправление: Копию DRY нужно делать в PluginProcessor или здесь?
        // Сделаем здесь для инкапсуляции.
        // Но чтобы не аллоцировать память каждый раз, MixEngine уже имеет internal buffer?

        // MixEngine.process требует входной буфер.
        // ВАЖНО: Мы не можем восстановить Dry здесь, он уже перезаписан.
        // РЕШЕНИЕ: Передаем Dry копию в аргументы processBlock или сохраняем её внутри.
        // Самый чистый вариант: PluginProcessor делает копию.
        // Но мы хотим инкапсуляции. Давайте сделаем сохранение Dry здесь.
    }

    // Перегрузка processBlock с явным Dry буфером (Best Practice)
    void processBlockWithDry(juce::AudioBuffer<float>& ioBuffer,
                             const juce::AudioBuffer<float>& dryBuffer,
                             const ParameterSet& params)
    {
        updateLatency(); // Проверяем, не изменилась ли задержка

        // 1. Network
        auto netModulations = networkController.process(dryBuffer, params);

        // 2. Upsample
        juce::dsp::AudioBlock<float> ioBlock(ioBuffer);
        auto highBlock = oversampler->processSamplesUp(ioBlock);

        // 3. Process
        filterBankEngine.process(highBlock, params, netModulations);

        // 4. Downsample
        oversampler->processSamplesDown(ioBlock);

        // 5. Mix (ioBuffer = Wet, dryBuffer = Dry)
        mixEngine.process(ioBlock, dryBuffer, params.mix, params.outputGain);
    }

    float getLatency() const { return currentLatency; }

private:
    void updateLatency()
    {
        // Latency = Oversampler (Up+Down) + FilterBank (scaled down)
        float osLatency = oversampler->getLatencyInSamples();
        float fbLatencyHigh = (float)filterBankEngine.getLatencySamples();
        float fbLatencyNorm = fbLatencyHigh / 4.0f; // Делим на фактор оверсемплинга

        float total = osLatency + fbLatencyNorm;

        if (std::abs(total - currentLatency) > 0.1f) {
            currentLatency = total;
            mixEngine.setLatencySamples(currentLatency);
        }
    }

    double sampleRate = 44100.0;
    juce::uint32 blockSize = 512;
    float currentLatency = 0.0f;

    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
    FilterBankEngine filterBankEngine;
    MixEngine mixEngine;
    NetworkController networkController;
};

} // namespace Cohera
