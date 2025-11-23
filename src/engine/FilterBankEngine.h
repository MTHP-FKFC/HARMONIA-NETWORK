#pragma once

#include "../CoheraTypes.h"
#include "../dsp/FilterBank.h" // Ваш кроссовер
#include "../parameters/ParameterSet.h"
#include "BandProcessingEngine.h" // Рабочая лошадка
#include <array>
#include <juce_dsp/juce_dsp.h>
#include <vector>

namespace Cohera {

class FilterBankEngine {
public:
  void prepare(const juce::dsp::ProcessSpec &spec) {
    sampleRate = spec.sampleRate;
    // Запоминаем, сколько мы обещали обработать
    currentMaxBlockSize = spec.maximumBlockSize;

    // 1. Настройка Кроссовера
    FilterBankConfig fbConfig;
    fbConfig.sampleRate = spec.sampleRate;
    fbConfig.maxBlockSize = spec.maximumBlockSize;
    fbConfig.numChannels = (int)spec.numChannels; // Add numChannels
    fbConfig.numBands = kNumBands;
    fbConfig.phaseMode = FilterPhaseMode::MinFIR128; // Safe mode (128 taps)
    fbConfig.profile = CrossoverProfile::Default;

    filterBank.prepare(fbConfig);

    // 2. Аллокация буферов полос
    // Важно: делаем это здесь, чтобы не аллоцировать в processBlock
    bandBufferPtrs.resize(kNumBands);
    for (int i = 0; i < kNumBands; ++i) {
      // +2 сэмпла запаса на всякий случай, 2 канала
      bandBuffers[i].setSize(2, (int)spec.maximumBlockSize + 2);
      bandBufferPtrs[i] = &bandBuffers[i];

      // Готовим движки полос
      bandEngines[i].prepare(spec);
    }

    // 3. Pre/Post Filters (Tone Shaping)
    // Используем TPT фильтры (State Variable), они отлично звучат и стабильны
    // при модуляции
    for (int ch = 0; ch < 2; ++ch) {
      preFilters[ch].prepare(spec);
      preFilters[ch].setType(juce::dsp::StateVariableTPTFilterType::highpass);

      postFilters[ch].prepare(spec);
      postFilters[ch].setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    }

    // Сглаживание частот фильтров
    smoothTighten.reset(spec.sampleRate, 0.05);
    smoothSmooth.reset(spec.sampleRate, 0.05);
  }

  void reset() {
    filterBank.reset();
    for (auto &engine : bandEngines)
      engine.reset();

    for (int ch = 0; ch < 2; ++ch) {
      preFilters[ch].reset();
      postFilters[ch].reset();
    }

    smoothTighten.setCurrentAndTargetValue(smoothTighten.getTargetValue());
    smoothSmooth.setCurrentAndTargetValue(smoothSmooth.getTargetValue());
  }

  int getLatencySamples() const { 
    if (sampleRate <= 0.0) return 0; // Not prepared yet
    return filterBank.getLatencySamples(); 
  }
  float getToneShapingLatencySamples() const {
    return toneShapingLatencyBaseSamples;
  }

  // Основной процесс: Вход -> Фильтр -> Сплит -> Обработка -> Сумма -> Фильтр
  // -> Выход ioBlock: входной сигнал, который будет перезаписан результатом
  // netModulations: массив из 6 значений (0..1) от NetworkManager
  float process(juce::dsp::AudioBlock<float> &ioBlock,
                const ParameterSet &params,
                const std::array<float, kNumBands> &netModulations) {
    // Safety check: ensure engine is prepared
    if (sampleRate <= 0.0 || currentMaxBlockSize == 0) {
      ioBlock.clear();
      return 0.0f;
    }
    
    const int numSamples = (int)ioBlock.getNumSamples();
    const int numChannels = (int)ioBlock.getNumChannels();
    
    // CRITICAL FIX: Validate channel count
    if (numChannels == 0 || numChannels > 2) {
      ioBlock.clear();
      return 0.0f;
    }

    // CRITICAL FIX: Clamp block size instead of expanding
    // Expanding currentMaxBlockSize doesn't actually resize buffers!
    if ((size_t)numSamples > currentMaxBlockSize) {
      juce::Logger::writeToLog(
        "WARNING: FilterBankEngine received block size (" + juce::String(numSamples) +
        ") larger than prepared (" + juce::String((int)currentMaxBlockSize) + ")! Clamping.");
      
      // Process only what we can handle safely
      auto safeBlock = ioBlock.getSubBlock(0, currentMaxBlockSize);
      return process(safeBlock, params, netModulations);
    }

    // --- 1. TONE SHAPING (PRE-FILTER / TIGHTEN) ---
    smoothTighten.setTargetValue(params.preFilterFreq);
    smoothSmooth.setTargetValue(params.postFilterFreq);

    // Применяем Tighten (HPF) к входящему сигналу
    // Чтобы не выделять память, процессим поканально in-place
    // OPTIMIZATION: Update filter cutoff every 16 samples instead of per-sample
    for (int i = 0; i < numSamples; ++i) {
      if (i % 16 == 0) { // Update cutoff every 16 samples
        float cutoff = smoothTighten.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch) {
          preFilters[ch].setCutoffFrequency(cutoff);
        }
      }
      // Прямой доступ к данным блока
      for (int ch = 0; ch < numChannels; ++ch) {
        float *data = ioBlock.getChannelPointer(ch);
        data[i] = preFilters[ch].processSample(ch, data[i]);
      }
    }

    // --- 2. SPLIT INTO BANDS ---
    // CRITICAL FIX: Validate we have valid band buffer pointers
    bool allBuffersValid = true;
    for (int b = 0; b < kNumBands; ++b) {
      if (bandBufferPtrs[b] == nullptr) {
        allBuffersValid = false;
        break;
      }
    }
    
    if (!allBuffersValid) {
      ioBlock.clear();
      return 0.0f;
    }
    
    // Создаем AudioBuffer обертку для FilterBank (он принимает AudioBuffer)
    // Для эффективности используем zero-copy обертку, ссылаясь на данные
    // ioBlock
    float *channelData[2]; // Max 2 channels supported
    for (int ch = 0; ch < numChannels; ++ch)
      channelData[ch] = ioBlock.getChannelPointer(ch);

    juce::AudioBuffer<float> inputWrapper(channelData, numChannels, numSamples);

    // Разделяем на полосы
    filterBank.splitIntoBands(inputWrapper, bandBufferPtrs.data(), numSamples);

    // --- 3. PROCESS BANDS ---

    // Настройка Tilt (меньше искажений на низах, больше на верхах)
    // 0(Sub) 1(Low) 2(LowMid) 3(Mid) 4(HighMid) 5(High)
    constexpr float kBandTilt[6] = {0.5f, 0.75f, 1.0f, 1.0f, 1.1f, 1.25f};

    float maxGlobalTransient = 0.0f;

    for (int b = 0; b < kNumBands; ++b) {
      // Создаем AudioBlock для текущей полосы
      juce::dsp::AudioBlock<float> bandBlock(bandBuffers[b]);
      // Берем под-блок нужной длины (важно, т.к. буфер может быть больше)
      auto subBlock = bandBlock.getSubBlock(0, (size_t)numSamples);

      // Запускаем процессинг полосы!
      // Передаем Tilt и сигнал модуляции
      float bandTrans = bandEngines[b].process(subBlock, params, kBandTilt[b],
                                               netModulations[b]);

      if (bandTrans > maxGlobalTransient)
        maxGlobalTransient = bandTrans;

      // Обновляем Gain Reduction для UI (RMS уровень полосы после обработки)
      float bandRMS = 0.0f;
      for (int ch = 0; ch < numChannels; ++ch) {
        bandRMS += subBlock.getChannelPointer(ch)[0] *
                   subBlock.getChannelPointer(ch)[0];
      }
      bandRMS = std::sqrt(bandRMS / numChannels);
      // Нормализуем относительно входного уровня (примерно)
      currentGR[b] = bandRMS > 0.0001f ? std::min(2.0f, bandRMS) : 1.0f;
    }

    // --- 4. SUM BANDS ---
    ioBlock.clear();

    for (int b = 0; b < kNumBands; ++b) {
      for (int ch = 0; ch < numChannels; ++ch) {
        // Складываем: Output[ch] += BandBuffer[b][ch]
        juce::FloatVectorOperations::add(
            ioBlock.getChannelPointer(ch),     // Dest
            bandBuffers[b].getReadPointer(ch), // Src
            numSamples                         // Count
        );
      }
    }

    // --- 4.5 PHYSICAL COMPENSATION ---
    // Исторически тут был множитель 0.35, но после перевода на новую
    // архитектуру сумма полос уже приблизительно совпадает по уровню с Dry
    // сигналом, поэтому дополнительное ослабление только портило импульсный
    // отклик.

    // --- 5. TONE SHAPING (POST-FILTER / SMOOTH) ---
    // OPTIMIZATION: Update filter cutoff every 16 samples instead of per-sample
    for (int i = 0; i < numSamples; ++i) {
      if (i % 16 == 0) { // Update cutoff every 16 samples
        float cutoff = smoothSmooth.getNextValue();
        for (int ch = 0; ch < numChannels; ++ch) {
          postFilters[ch].setCutoffFrequency(cutoff);
        }
      }
      for (int ch = 0; ch < numChannels; ++ch) {
        float *data = ioBlock.getChannelPointer(ch);
        data[i] = postFilters[ch].processSample(ch, data[i]);
      }
    }

    return maxGlobalTransient;
  }

private:
  double sampleRate = 0.0; // 0 means not prepared
  size_t currentMaxBlockSize = 0; // 0 means not prepared

  // DSP
  PlaybackFilterBank filterBank;
  std::array<BandProcessingEngine, kNumBands> bandEngines;

  // Buffers
  std::array<juce::AudioBuffer<float>, kNumBands> bandBuffers;
  std::vector<juce::AudioBuffer<float> *>
      bandBufferPtrs; // Для передачи в filterBank

  // Filters
  std::array<juce::dsp::StateVariableTPTFilter<float>, 2> preFilters;
  std::array<juce::dsp::StateVariableTPTFilter<float>, 2> postFilters;

  juce::LinearSmoothedValue<float> smoothTighten;
  juce::LinearSmoothedValue<float> smoothSmooth;

  // Gain Reduction для UI метров (обновляется в process)
  std::array<float, 6> currentGR{1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f};

  // Derived from measuring the TPT pre/post filters plus band DC-blockers in
  // isolation. The processing runs at 4x rate, but once downsampled it behaves
  // like an extra ~25.5 samples of latency.
  static constexpr float toneShapingLatencyBaseSamples = 25.5f;

public:
  const std::array<float, 6> &getGainReductionValues() const {
    return currentGR;
  }
  
  // UI Metrics: Average thermal temperature across all 6 bands
  float getAverageTemperature() const {
    float totalTemp = 0.0f;
    for (const auto& bandEngine : bandEngines) {
      totalTemp += bandEngine.getTemperature();
    }
    return totalTemp / 6.0f;
  }
};

} // namespace Cohera
