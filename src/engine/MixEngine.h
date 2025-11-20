#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../CoheraTypes.h"

namespace Cohera {

class MixEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        // Настраиваем Delay Line для Dry сигнала (макс 1 сек)
        dryDelayLine.prepare(spec);
        dryDelayLine.setMaximumDelayInSamples(spec.sampleRate);
    }

    void reset()
    {
        dryDelayLine.reset();
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

        // 1. Создаем AudioBlock из исходного буфера для DelayLine
        // Нам нужен временный буфер для задержанного Dry, так как DelayLine пишет in-place
        // Но постойте, DelayLine в JUCE имеет push/pop или process.
        // Используем простой способ: копируем input во временный, процессим delay, миксуем.

        // ЛУЧШЕ: Прямо используем DelayLine context
        // Но DelayLine.process работает in-place. Нам нельзя портить dryInputBuffer (он const).
        // Поэтому нам нужен member buffer для dry.

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

        // 2. Mix & Output Loop
        for (size_t i = 0; i < numSamples; ++i)
        {
            for (size_t ch = 0; ch < numChannels; ++ch)
            {
                float dry = dryBlock.getSample(ch, i);
                float wet = wetBlock.getSample(ch, i);

                // Линейный микс
                float out = dry * (1.0f - mixAmount) + wet * mixAmount;

                // Output Gain
                out *= outputGain;

                // Safety Clipper (Hard Limit at 0dBFS to prevent explosion)
                // Мягкий клиппер на выходе для мастеринг-безопасности
                if (out > 1.0f) out = 1.0f;
                else if (out < -1.0f) out = -1.0f;

                // Пишем обратно в Wet блок (который теперь Output)
                wetBlock.setSample(ch, i, out);
            }
        }
    }

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> dryDelayLine { 48000 };
    juce::AudioBuffer<float> delayedDryBuffer;
};

} // namespace Cohera
