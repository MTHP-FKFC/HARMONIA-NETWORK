#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../CoheraTypes.h"
#include "../dsp/AutoGainStage.h"
#include "../dsp/DCBlocker.h"

namespace Cohera {

class MixEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        // Настраиваем Delay Line для Dry сигнала (макс 1 сек)
        dryDelayLine.prepare(spec);
        dryDelayLine.setMaximumDelayInSamples(spec.sampleRate);

        // Настраиваем AutoGain
        autoGain.prepare(spec.sampleRate);
    }

    void reset()
    {
        dryDelayLine.reset();
        autoGain.resetStates();
        dcBlockerLeft.reset();
        dcBlockerRight.reset();
    }

    // Установка задержки (должна вызываться при изменении latency фильтров)
    void setLatencySamples(float samples)
    {
        // Небольшая защита, чтобы не ставить отрицательную
        if (samples < 0) samples = 0;
        dryDelayLine.setDelay(samples);
    }

    // Смешивает Wet (processed) с Dry (из delay line).
    // ВАЖНО: wetBlock содержит обработанный сигнал. dryInputBuffer - исходный вход.
    // Результат пишется в wetBlock (in-place output).
    void process(juce::dsp::AudioBlock<float>& wetBlock,
                 const juce::AudioBuffer<float>& dryInputBuffer,
                 float mixAmount,
                 float outputGain)
    {
        size_t numSamples = wetBlock.getNumSamples();
        size_t numChannels = wetBlock.getNumChannels();

        // 0. AutoGain: Анализируем вход перед обработкой
        autoGain.analyzeInput(dryInputBuffer);

        // Re-allocate dry buffer if needed (cheap check)
        if (delayedDryBuffer.getNumSamples() != numSamples || delayedDryBuffer.getNumChannels() != numChannels) {
            delayedDryBuffer.setSize((int)numChannels, (int)numSamples);
        }

        // Копируем вход
        for (size_t ch = 0; ch < numChannels; ++ch) {
             delayedDryBuffer.copyFrom((int)ch, 0, dryInputBuffer.getReadPointer((int)ch), (int)numSamples);
        }

        // Применяем задержку к копии
        juce::dsp::AudioBlock<float> dryBlock(delayedDryBuffer);
        juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);
        dryDelayLine.process(dryContext);

        // 1. Mix & AutoGain Loop
        for (size_t i = 0; i < numSamples; ++i)
        {
            // Получаем компенсацию уровня для этого сэмпла
            float autoGainValue = autoGain.getNextValue();

            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                float dry = dryBlock.getSample(ch, i);
                float wet = wetBlock.getSample(ch, i);

                // Линейный микс
                float out = dry * (1.0f - mixAmount) + wet * mixAmount;

                // AutoGain компенсация (применяется к миксовому сигналу)
                out *= autoGainValue;

                // Output Gain
                out *= outputGain;

                // Safety Clipper (Hard Limit at 0dBFS to prevent explosion)
                if (out > 1.0f) out = 1.0f;
                else if (out < -1.0f) out = -1.0f;

                // Final DC Blocker (последний рубеж против DC после сатурации)
                if (ch == 0)
                    out = dcBlockerLeft.process(out);
                else if (ch == 1)
                    out = dcBlockerRight.process(out);

                // Пишем обратно в Wet блок (который теперь Output)
                wetBlock.setSample(ch, i, out);
            }
        }

        // 2. AutoGain: Обновляем состояние после обработки
        // Создаем временный буфер для анализа выхода
        juce::AudioBuffer<float> outputAnalysisBuffer((int)numChannels, (int)numSamples);
        for (size_t ch = 0; ch < numChannels; ++ch) {
            outputAnalysisBuffer.copyFrom((int)ch, 0, wetBlock.getChannelPointer(ch), (int)numSamples);
        }
        autoGain.updateGainState(outputAnalysisBuffer);
    }

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> dryDelayLine { 48000 };
    juce::AudioBuffer<float> delayedDryBuffer;
    AutoGainStage autoGain;
    DCBlocker dcBlockerLeft, dcBlockerRight;
};

} // namespace Cohera
