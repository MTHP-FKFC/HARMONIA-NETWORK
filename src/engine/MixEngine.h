#pragma once

#include <juce_dsp/juce_dsp.h>
#include <iostream>
#include <fstream>
#include <cmath>
#include "../CoheraTypes.h"
#include "../dsp/PsychoAcousticGain.h"
#include "../dsp/DCBlocker.h"
#include "../dsp/StereoFocus.h"

namespace Cohera {

class MixEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec)
    {
        // Настраиваем Delay Line для Dry сигнала (макс 1 сек)
        dryDelayLine.prepare(spec);
        dryDelayLine.setMaximumDelayInSamples(spec.sampleRate);

        // Настраиваем PsychoAcoustic Auto-Gain (LUFS matching)
        psychoGain.prepare(spec.sampleRate);
    }

    void reset()
    {
        dryDelayLine.reset();
        psychoGain.reset();
        dcBlockerLeft.reset();
        dcBlockerRight.reset();
    }

    // Установка задержки (должна вызываться при изменении latency фильтров)
    void setLatencySamples(float samples)
    {
        if (samples < 0.0f)
            samples = 0.0f;

        juce::String mixLine = juce::String::formatted("[MixEngineDiag] Setting dry delay to %.3f samples\n", samples);
        juce::Logger::writeToLog(mixLine);
        std::cerr << mixLine.toStdString();
        juce::File mixLog = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("cohera_test_results.txt");
        if (mixLog.exists())
            mixLog.appendText(mixLine, false, false);

        // Also append to local workspace log file for diagnostics
        {
            std::ofstream ofs("build/latency_diag.log", std::ios::app);
            if (ofs.is_open()) {
                ofs << mixLine.toStdString();
                ofs.close();
            }
        }
        currentDelaySamples = samples;
        dryDelayLine.setDelay(currentDelaySamples);
    }

    // Смешивает Wet (processed) с Dry (из delay line).
    // ВАЖНО: wetBlock содержит обработанный сигнал. dryInputBuffer - исходный вход.
    // Результат пишется в wetBlock (in-place output).
    void process(juce::dsp::AudioBlock<float>& wetBlock,
                 const juce::AudioBuffer<float>& dryInputBuffer,
                 float mixAmount,
                 float outputGain,
                 float focus)
    {
        size_t numSamples = wetBlock.getNumSamples();
        size_t numChannels = wetBlock.getNumChannels();

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
        dryDelayLine.setDelay(currentDelaySamples);
        dryDelayLine.process(dryContext);

        // Main Loop: LUFS Gain -> Mix -> Output
        for (size_t i = 0; i < numSamples; ++i)
        {
            // 1. Читаем сэмплы
            float dryL = dryBlock.getSample(0, i);
            float dryR = (numChannels > 1) ? dryBlock.getSample(1, i) : dryL;
            
            float wetL = wetBlock.getSample(0, i);
            float wetR = (numChannels > 1) ? wetBlock.getSample(1, i) : wetL;

            // 2. PSYCHOACOUSTIC MATCHING
            // Сравниваем громкость Dry и Wet так, как это слышит ухо.
            // Получаем множитель, который нужно применить к Wet.
            float compensation = psychoGain.processStereoSample(dryL, dryR, wetL, wetR);
            
            // Применяем компенсацию к Wet
            wetL *= compensation;
            wetR *= compensation;

            // 3. MIX (Линейный)
            float outL = dryL * (1.0f - mixAmount) + wetL * mixAmount;
            float outR = dryR * (1.0f - mixAmount) + wetR * mixAmount;

            // 4. OUTPUT GAIN
            outL *= outputGain;
            outR *= outputGain;

            // 5. SAFETY LIMITER (Hard Limit at ±1.0 to prevent explosion)
            if (outL > 1.0f) outL = 1.0f;
            else if (outL < -1.0f) outL = -1.0f;
            
            if (outR > 1.0f) outR = 1.0f;
            else if (outR < -1.0f) outR = -1.0f;

            // 5.5. STEREO FOCUS (M/S Processing)
            if (focus != 0.0f && numChannels > 1)
            {
                // M/S Encoding
                float mid = 0.5f * (outL + outR);
                float side = 0.5f * (outL - outR);

                // Get multipliers based on focus (-100 = Mid only, +100 = Side only)
                auto multipliers = stereoFocus.getDriveScalars(focus * 100.0f);

                // Apply multipliers
                mid *= multipliers.midScale;
                side *= multipliers.sideScale;

                // M/S Decoding
                outL = mid + side;
                outR = mid - side;
            }

            // 6. FINAL DC BLOCKER (последний рубеж против DC после сатурации)
            outL = dcBlockerLeft.process(outL);
            outR = dcBlockerRight.process(outR);

            // Запись
            wetBlock.setSample(0, i, outL);
            if (numChannels > 1) wetBlock.setSample(1, i, outR);
        }
    }

private:
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> dryDelayLine { 48000 };
    juce::AudioBuffer<float> delayedDryBuffer;
    PsychoAcousticGain psychoGain;
    DCBlocker dcBlockerLeft, dcBlockerRight;
    StereoFocus stereoFocus;
    float currentDelaySamples = 0.0f;
};

} // namespace Cohera
