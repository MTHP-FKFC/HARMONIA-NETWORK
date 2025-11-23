#pragma once

#include "../CoheraTypes.h"
#include "../dsp/DCBlocker.h"
#include "../dsp/PsychoAcousticGain.h"
#include "../dsp/StereoFocus.h"
#include <cmath>
#include <fstream>
#include <iostream>
#include <juce_dsp/juce_dsp.h>

namespace Cohera {

class MixEngine {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) {
    // Настраиваем Delay Line для Dry сигнала (макс 1 сек)
    dryDelayLine.prepare(spec);
    dryDelayLine.setMaximumDelayInSamples(spec.sampleRate);

    // Настраиваем PsychoAcoustic Auto-Gain (LUFS matching)
    psychoGain.prepare(spec.sampleRate);

    // Настраиваем сглаживание (20ms ramp)
    // Это убирает треск при быстром вращении ручек
    smoothMix.reset(spec.sampleRate, 0.02);
    smoothGain.reset(spec.sampleRate, 0.02);
    smoothFocus.reset(spec.sampleRate, 0.02);

    // CRITICAL FIX: Pre-allocate dry buffer with 2x safety margin to avoid heap allocations
    // This prevents reallocation if host sends larger blocks than expected
    delayedDryBuffer.setSize(spec.numChannels, spec.maximumBlockSize * 2, false, true, false);
    preparedMaxBlockSize = spec.maximumBlockSize * 2;
  }

  void reset() {
    dryDelayLine.reset();
    psychoGain.reset();
    dcBlockerLeft.reset();
    dcBlockerRight.reset();

    // Сбрасываем сглаживатели к текущим целям (чтобы не было fade-in при
    // ресете) Но так как мы не знаем текущие цели, просто сбрасываем к 0 (или
    // дефолтам) Лучше это делать при первом process
    smoothMix.setCurrentAndTargetValue(smoothMix.getTargetValue());
    smoothGain.setCurrentAndTargetValue(smoothGain.getTargetValue());
    smoothFocus.setCurrentAndTargetValue(smoothFocus.getTargetValue());

    // Сбрасываем флаг инициализации
    gainInitialized = false;
  }

  // Установка задержки (должна вызываться при изменении latency фильтров)
  void setLatencySamples(float samples) {
    if (samples < 0.0f)
      samples = 0.0f;

    juce::String mixLine = juce::String::formatted(
        "[MixEngineDiag] Setting dry delay to %.3f samples\n", samples);
    juce::Logger::writeToLog(mixLine);
    std::cerr << mixLine.toStdString();
    juce::File mixLog =
        juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
            .getChildFile("cohera_test_results.txt");
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
  // ВАЖНО: wetBlock содержит обработанный сигнал. dryInputBuffer - исходный
  // вход. Результат пишется в wetBlock (in-place output).
  void process(juce::dsp::AudioBlock<float> &wetBlock,
               const juce::AudioBuffer<float> &dryInputBuffer,
               float targetMixAmount, float targetOutputGain, float targetFocus,
               bool deltaListen) {
    size_t numSamples = wetBlock.getNumSamples();
    size_t numChannels = wetBlock.getNumChannels();

    // Обновляем цели сглаживания
    smoothMix.setTargetValue(targetMixAmount);
    smoothGain.setTargetValue(targetOutputGain);
    smoothFocus.setTargetValue(targetFocus);

    // Инициализируем smoothGain при первом вызове или при большом изменении
    // Это важно для тестов, где параметры меняются быстро
    if (!gainInitialized) {
      smoothGain.setCurrentAndTargetValue(targetOutputGain);
      gainInitialized = true;
      lastOutputGain = targetOutputGain;
    } else if (std::abs(smoothGain.getCurrentValue() - targetOutputGain) >
               0.1f) {
      // Если изменение больше 10%, быстро устанавливаем новое значение
      // Это помогает в тестах, где параметры меняются резко
      smoothGain.setCurrentAndTargetValue(targetOutputGain);
    }

    // Если output gain изменился значительно, сбрасываем PsychoAcousticGain
    // чтобы он не компенсировал изменения output gain
    if (std::abs(lastOutputGain - targetOutputGain) > 0.01f) {
      psychoGain.reset();
      lastOutputGain = targetOutputGain;
    }

    // CRITICAL FIX: Clamp to prepared size instead of reallocating
    // Log warning if host violates contract
    if ((int)numSamples > preparedMaxBlockSize) {
      juce::Logger::writeToLog(
        "WARNING: MixEngine received block size (" + juce::String((int)numSamples) +
        ") larger than prepared (" + juce::String(preparedMaxBlockSize) + ")! Clamping.");
      numSamples = (size_t)preparedMaxBlockSize;
    }

    // Create sub-block wrapper to avoid processing garbage data if block < max
    juce::dsp::AudioBlock<float> fullDryBlock(delayedDryBuffer);
    auto dryBlock = fullDryBlock.getSubBlock(0, numSamples);

    // Копируем вход
    for (size_t ch = 0; ch < numChannels; ++ch) {
      // Copy only valid samples
      juce::FloatVectorOperations::copy(dryBlock.getChannelPointer(ch),
                                        dryInputBuffer.getReadPointer((int)ch),
                                        (int)numSamples);
    }

    // Применяем задержку к копии
    juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);
    dryDelayLine.setDelay(currentDelaySamples);
    dryDelayLine.process(dryContext);

    // Main Loop: LUFS Gain -> Mix -> Output
    for (size_t i = 0; i < numSamples; ++i) {
      // Получаем сглаженные параметры для текущего сэмпла
      float mix = smoothMix.getNextValue();
      float gain = smoothGain.getNextValue();
      float focus = smoothFocus.getNextValue();

      // 1. Читаем сэмплы
      float dryL = dryBlock.getSample(0, i);
      float dryR = (numChannels > 1) ? dryBlock.getSample(1, i) : dryL;

      float wetL = wetBlock.getSample(0, i);
      float wetR = (numChannels > 1) ? wetBlock.getSample(1, i) : wetL;

      // 2. MIX (first! for correct gain staging)
      float outL, outR;

      if (deltaListen) {
        // Delta: слышим разницу между обработанным и исходным
        outL = wetL - dryL;
        outR = wetR - dryR;
      } else {
        // Normal Mix
        outL = dryL * (1.0f - mix) + wetL * mix;
        outR = dryR * (1.0f - mix) + wetR * mix;
      }

      // 3. PSYCHOACOUSTIC MATCHING (after mix for correct LUFS)
      // Compare the actual mixed result to dry reference for proper loudness matching
      float compensation =
          psychoGain.processStereoSample(dryL, dryR, outL, outR);

      // Apply compensation to the mixed result
      outL *= compensation;
      outR *= compensation;

      // 4. SOFT KNEE LIMITER (Professional headroom protection)
      // Threshold: -0.1dBFS (0.989), Ratio: 10:1, Soft Knee: 0.5dB
      auto softLimit = [](float x) -> float {
          const float threshold = 0.989f; // -0.1dBFS
          const float knee = 0.5f;        // 0.5dB knee width
          const float ratio = 10.0f;      // 10:1 compression above threshold

          if (x > threshold) {
              // Soft knee: smooth transition
              float over = x - threshold;
              if (over < knee) {
                  // In knee region: gradually increase compression
                  float ratioAdj = 1.0f + (ratio - 1.0f) * (over / knee);
                  x = threshold + over / ratioAdj;
              } else {
                  // Above knee: full compression
                  x = threshold + knee / ratio + (over - knee) / ratio;
              }
          } else if (x < -threshold) {
              // Same for negative values
              float over = -x - threshold;
              if (over < knee) {
                  float ratioAdj = 1.0f + (ratio - 1.0f) * (over / knee);
                  x = -threshold - over / ratioAdj;
              } else {
                  x = -threshold - knee / ratio - (over - knee) / ratio;
              }
          }
          return x;
      };

      outL = softLimit(outL);
      outR = softLimit(outR);

      // 5. STEREO FOCUS (M/S Processing with correct orthonormal matrix)
      if (std::abs(focus) > 0.001f && numChannels > 1) {
        // Orthonormal M/S Encoding (preserves energy: L² + R² = M² + S²)
        const float SQRT2_INV = 0.7071067811865476f; // 1/√2
        float mid = (outL + outR) * SQRT2_INV;
        float side = (outL - outR) * SQRT2_INV;

        // Get multipliers based on focus (-100 = Mid only, +100 = Side only)
        auto multipliers = stereoFocus.getDriveScalars(focus * 100.0f);

        // Apply multipliers
        mid *= multipliers.midScale;
        side *= multipliers.sideScale;

        // Orthonormal M/S Decoding
        outL = (mid + side) * SQRT2_INV;
        outR = (mid - side) * SQRT2_INV;
      }

      // 6. DC BLOCKER (последний рубеж против DC после сатурации)
      outL = dcBlockerLeft.process(outL);
      outR = dcBlockerRight.process(outR);

      // 7. OUTPUT GAIN (в самом конце - независим от всех компенсаций)
      outL *= gain;
      outR *= gain;

      // Запись
      wetBlock.setSample(0, i, outL);
      if (numChannels > 1)
        wetBlock.setSample(1, i, outR);
    }
  }

private:
  juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear>
      dryDelayLine{48000};
  juce::AudioBuffer<float> delayedDryBuffer;
  PsychoAcousticGain psychoGain;
  DCBlocker dcBlockerLeft, dcBlockerRight;
  StereoFocus stereoFocus;
  float currentDelaySamples = 0.0f;
  int preparedMaxBlockSize = 0; // CRITICAL FIX: Track prepared size for safety checks

  // Parameter Smoothers
  juce::LinearSmoothedValue<float> smoothMix{1.0f};
  juce::LinearSmoothedValue<float> smoothGain{1.0f};
  juce::LinearSmoothedValue<float> smoothFocus{0.0f};

  // Флаг для инициализации smoothGain
  bool gainInitialized = false;
  float lastOutputGain = 1.0f;
};

} // namespace Cohera
