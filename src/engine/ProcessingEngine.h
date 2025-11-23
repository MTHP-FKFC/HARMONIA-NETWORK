#pragma once

#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
#include <atomic>
#include <cmath>
#include <juce_dsp/juce_dsp.h>
#include <vector>

// Наши компоненты
#include "../network/NetworkController.h"
#include "FilterBankEngine.h"
#include "MixEngine.h"

namespace Cohera {

class ProcessingEngine {
public:
  ProcessingEngine() {
    // Инициализируем Oversampler (4x, Linear Phase)
    // Используем filterHalfBandFIREquiripple для максимальной фазовой
    // линейности
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(
        2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple,
        true);
    numChannels = 2;
  }

  void prepare(const juce::dsp::ProcessSpec &spec) {
    // Защита от дурака
    if (spec.sampleRate <= 0)
      return;

    sampleRate = spec.sampleRate;
    blockSize = spec.maximumBlockSize;
    numChannels = (int)spec.numChannels;

    // 1. Resetting Oversampler
    // reset() важен, чтобы очистить внутренние буферы
    oversampler->reset();

    // initProcessing выделяет память под буферы.
    // Если blockSize увеличился, он перевыделит.
    oversampler->initProcessing(blockSize);

    // Вычисляем параметры для High Rate (4x)
    double highSampleRate = sampleRate * 4.0;
    int highBlockSize = (int)blockSize * 4;

    juce::dsp::ProcessSpec highSpec{highSampleRate, (juce::uint32)highBlockSize,
                                    spec.numChannels};

    // 2. Готовим компоненты
    filterBankEngine.prepare(highSpec);    // Работает на 4x
    mixEngine.prepare(spec);               // Работает на 1x (Output)
    networkController.prepare(sampleRate); // Работает на 1x (Analysis)

    updateLatencyFromComponents();
  }

  void reset() {
    oversampler->reset();
    filterBankEngine.reset();  // Сброс фильтров и буферов
    mixEngine.reset();         // Сброс delay line
    networkController.reset(); // Сброс энвелопов

    // Сброс атомиков для UI
    inputRMS.store(0.0f);
    outputRMS.store(0.0f);
    lastTransientLevel.store(0.0f);
    lastRefSignal.store(0.0f);
    lastDepthValue.store(0.0f);
  }

  // Главный метод, вызываемый из PluginProcessor
  // (Оставляем без изменений, логика уже верная)
  void processBlockWithDry(juce::AudioBuffer<float> &ioBuffer,
                           const juce::AudioBuffer<float> &dryBuffer,
                           const ParameterSet &params) {
    // ... (код processBlockWithDry остается тем же, что был ранее) ...
    // ВАЖНО: Мы убираем вызов updateLatency() отсюда, чтобы не пересчитывать
    // его каждый блок, или делаем его очень легким. Для динамической смены
    // качества (Eco/Pro) калибровку нужно вызывать при смене параметра.

    // 0. RMS измерение
    float rms = 0.0f;
    for (int ch = 0; ch < dryBuffer.getNumChannels(); ++ch)
      rms += dryBuffer.getRMSLevel(ch, 0, dryBuffer.getNumSamples());
    rms /= (float)std::max(1, dryBuffer.getNumChannels());
    inputRMS.store(rms);

    // 1. Network
    auto netModulations = networkController.process(dryBuffer, params);

    // Export Network Data for UI
    float maxRef = 0.0f;
    for (float v : netModulations)
      if (v > maxRef)
        maxRef = v;
    lastRefSignal.store(maxRef);

    // 2. Upsample
    juce::dsp::AudioBlock<float> ioBlock(ioBuffer);
    auto highBlock = oversampler->processSamplesUp(ioBlock);

    // 3. Process
    float transientLevel =
        filterBankEngine.process(highBlock, params, netModulations);
    lastTransientLevel.store(transientLevel);

    // Export Modulation Depth
    auto grs = filterBankEngine.getGainReductionValues();
    float totalAct = 0.0f;
    for (float g : grs)
      totalAct += std::abs(1.0f - g);
    lastDepthValue.store(std::min(1.0f, totalAct));

    // 4. Downsample
    oversampler->processSamplesDown(ioBlock);

    // 5. Mix (ioBuffer = Wet, dryBuffer = Dry)
    // mixEngine already configured with the calibrated latency
    mixEngine.process(ioBlock, dryBuffer, params.mix, params.outputGain,
                      params.focus, params.deltaListen);

    // 6. Output RMS
    float outRms = 0.0f;
    for (int ch = 0; ch < ioBuffer.getNumChannels(); ++ch)
      outRms += ioBuffer.getRMSLevel(ch, 0, ioBuffer.getNumSamples());
    outRms /= (float)std::max(1, ioBuffer.getNumChannels());
    outputRMS.store(outRms);
  }

  float getLatency() const { return currentLatency; }

  // Геттеры для UI
  float getInputRMS() const { return inputRMS.load(); }
  float getOutputRMS() const { return outputRMS.load(); }
  float getTransientLevel() const { return lastTransientLevel.load(); }
  float getLastReferenceSignal() const { return lastRefSignal.load(); }
  float getLastModulationDepth() const { return lastDepthValue.load(); }

  const std::array<float, 6> &getGainReductionValues() const {
    return filterBankEngine.getGainReductionValues();
  }

private:
  void updateLatencyFromComponents() {
    const float oversampleLatency =
        oversampler->getLatencyInSamples(); // уже в базовой частоте
    const float fbLatencyHigh = (float)filterBankEngine.getLatencySamples();
    const float fbLatencyBase = fbLatencyHigh / oversamplingFactor;
    const float toneLatency = filterBankEngine.getToneShapingLatencySamples();

    currentLatency = oversampleLatency + fbLatencyBase + toneLatency;
    mixEngine.setLatencySamples(currentLatency);

    juce::Logger::writeToLog(
        "Latency (analytic): os=" + juce::String(oversampleLatency, 3) +
        " fbHigh=" + juce::String(fbLatencyHigh, 3) +
        " fbBase=" + juce::String(fbLatencyBase, 3) +
        " tone=" + juce::String(toneLatency, 3) +
        " total=" + juce::String(currentLatency, 3));
  }

  double sampleRate = 44100.0;
  juce::uint32 blockSize = 512;
  int numChannels = 2;
  float currentLatency = 0.0f;

  // Атомики для UI
  std::atomic<float> inputRMS{0.0f};
  std::atomic<float> outputRMS{0.0f};
  std::atomic<float> lastTransientLevel{0.0f};
  std::atomic<float> lastRefSignal{0.0f};
  std::atomic<float> lastDepthValue{0.0f};

  static constexpr float oversamplingFactor = 4.0f;
  std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
  FilterBankEngine filterBankEngine;
  MixEngine mixEngine;
  NetworkController networkController;
};

} // namespace Cohera
