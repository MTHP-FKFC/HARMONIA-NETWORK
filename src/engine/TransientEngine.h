#pragma once

#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
#include <array>
#include <juce_dsp/juce_dsp.h>
// Подключаем DSP модули
#include "../dsp/MathSaturator.h"
#include "../dsp/TransientSplitter.h"

namespace Cohera {

class TransientEngine {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) {
    for (auto &s : splitters) {
      s.prepare(spec.sampleRate);
      s.reset();
    }

    smoothedPunch.reset(spec.sampleRate,
                        0.001); // 1ms для быстрой реакции на транзиенты
    // Don't reset punch to 0 to allow immediate response in tests

    smoothedDrive.reset(spec.sampleRate, 0.02); // 20ms ramp for drive
    smoothedDrive.setCurrentAndTargetValue(1.0f);
  }

  void reset() {
    for (auto &s : splitters)
      s.reset();
    // Don't reset punch to 0 to allow immediate response in tests
    smoothedDrive.setCurrentAndTargetValue(1.0f);
    firstBlock = true;
  }

  // Основной процессинг
  float process(juce::dsp::AudioBlock<float> &block, const ParameterSet &params,
                float driveMult = 1.0f) {
    smoothedPunch.setCurrentAndTargetValue(params.punch);

    // Эффективный драйв (база * модуляция извне)
    float targetBaseDrive = params.getEffectiveDriveGain() * driveMult;

    if (firstBlock) {
      smoothedDrive.setCurrentAndTargetValue(targetBaseDrive);
      firstBlock = false;
    } else {
      smoothedDrive.setTargetValue(targetBaseDrive);
    }

    size_t numSamples = block.getNumSamples();
    size_t numChannels = block.getNumChannels();

    float maxTransient = 0.0f;

    for (size_t i = 0; i < numSamples; ++i) {
      float punchVal = smoothedPunch.getNextValue();
      float currentDrive = smoothedDrive.getNextValue();
      bool isNeutral = std::abs(punchVal) < 0.01f;

      for (size_t ch = 0; ch < numChannels; ++ch) {
        // Защита от выхода за пределы массива сплиттеров (у нас их 2)
        if (ch >= 2)
          break;

        float *data = block.getChannelPointer(ch);
        float input = data[i];

        if (isNeutral) {
          // 1. Просто Сатурация (экономим CPU на сплите)
          data[i] = mathSaturator.processSample(input, currentDrive,
                                                params.saturationMode);
        } else {
          // 2. Split & Crush
          auto split = splitters[ch].process(input);

          // Detect transient peak for UI
          float absTrans = std::abs(split.trans);
          if (absTrans > maxTransient)
            maxTransient = absTrans;

          // Body: всегда основной алгоритм
          float processedBody = mathSaturator.processSample(
              split.body, currentDrive, params.saturationMode);

          // Transient: зависит от знака Punch
          float processedTrans = 0.0f;

          if (punchVal > 0.0f) // Dirty Attack
          {
            float transDrive = currentDrive * (1.0f + punchVal * 2.0f);
            processedTrans = mathSaturator.processSample(
                split.trans, transDrive, params.saturationMode);
          } else // Clean Attack
          {
            float transDrive =
                currentDrive * (1.0f - std::abs(punchVal) * 0.8f);
            // Для чистоты используем EulerTube (или Clean, если нужно совсем
            // прозрачно)
            processedTrans = mathSaturator.processSample(
                split.trans, transDrive, SaturationMode::EulerTube);
          }

          // Сумма
          data[i] = processedBody + processedTrans;
        }
      }
    }
    return maxTransient;
  }

private:
  std::array<TransientSplitter, 2> splitters;
  MathSaturator mathSaturator;
  juce::SmoothedValue<float> smoothedPunch;
  juce::LinearSmoothedValue<float> smoothedDrive{1.0f};
  bool firstBlock = true;
};

} // namespace Cohera
