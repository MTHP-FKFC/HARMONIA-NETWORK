#pragma once

#include "ParameterSet.h"
#include <juce_audio_processors/juce_audio_processors.h>

using namespace Cohera;

namespace Cohera {

class ParameterManager {
public:
  ParameterManager(juce::AudioProcessorValueTreeState &apvtsRef)
      : apvts(apvtsRef) {
    // Кэшируем указатели на параметры для скорости (вместо getRawParameterValue
    // по строке)
    pDrive = apvts.getRawParameterValue("drive_master");
    pMix = apvts.getRawParameterValue("mix");
    pOutput = apvts.getRawParameterValue("output_gain");
    pMode = apvts.getRawParameterValue("math_mode");
    pQuality = apvts.getRawParameterValue("quality");
    pTighten = apvts.getRawParameterValue("tone_tighten");
    pSmooth = apvts.getRawParameterValue("tone_smooth");
    pPunch = apvts.getRawParameterValue("punch");
    pDynamics = apvts.getRawParameterValue("dynamics");
    pNetDepth = apvts.getRawParameterValue("net_depth");
    pNetSmooth = apvts.getRawParameterValue("net_smooth");
    pNetSens = apvts.getRawParameterValue("net_sens");
    pHeat = apvts.getRawParameterValue("heat_amount");
    pDrift = apvts.getRawParameterValue("analog_drift");
    pVariance = apvts.getRawParameterValue("variance");
    pEntropy = apvts.getRawParameterValue("entropy");
    pNoise = apvts.getRawParameterValue("noise");
    pFocus = apvts.getRawParameterValue("focus");
    pGroup = apvts.getRawParameterValue("group_id");
    pRole = apvts.getRawParameterValue("role");
    pNetMode = apvts.getRawParameterValue("mode");
    pNetReaction = apvts.getRawParameterValue("net_reaction");
    pCascade = apvts.getRawParameterValue("cascade");
    pDelta = apvts.getRawParameterValue("delta");
  }

  // Вызывается в начале processBlock
  ParameterSet getCurrentParams() const {
    ParameterSet params;

    // OPTIMIZATION: Use memory_order_relaxed for real-time audio thread.
    // We don't need sequential consistency between parameters.
    const auto order = std::memory_order_relaxed;

    // Читаем атомики
    params.drive = pDrive->load(order);
    params.mix = pMix->load(order) / 100.0f;
    params.outputGain = juce::Decibels::decibelsToGain(pOutput->load(order));

    params.saturationMode = static_cast<SaturationMode>((int)pMode->load(order));
    params.qualityMode = static_cast<QualityMode>((int)pQuality->load(order));
    params.cascade = pCascade->load(order) > 0.5f;
    params.deltaListen = pDelta->load(order) > 0.5f;

    params.preFilterFreq = pTighten->load(order);
    params.postFilterFreq = pSmooth->load(order);
    params.punch = pPunch->load(order) / 100.0f;
    params.dynamics = pDynamics->load(order) / 100.0f;

    params.netDepth = pNetDepth->load(order) / 100.0f;
    params.netSmooth = pNetSmooth->load(order) / 100.0f;
    params.netSens = pNetSens->load(order) / 100.0f;

    params.globalHeat = pHeat->load(order) / 100.0f;
    params.analogDrift = pDrift->load(order) / 100.0f;
    params.variance = pVariance->load(order) / 100.0f;
    params.entropy = pEntropy->load(order) / 100.0f;
    params.noise = pNoise->load(order) / 100.0f;
    params.focus = pFocus->load(order) / 100.0f;

    params.groupId = (int)pGroup->load(order);
    params.netRole = static_cast<NetworkRole>((int)pRole->load(order));
    params.netMode = static_cast<NetworkMode>((int)pNetMode->load(order));
    params.netReaction = static_cast<NetReaction>((int)pNetReaction->load(order));

    return params;
  }

private:
  juce::AudioProcessorValueTreeState &apvts;

  // Кэшированные указатели (std::atomic<float>*)
  std::atomic<float> *pDrive = nullptr;
  std::atomic<float> *pMix = nullptr;
  std::atomic<float> *pOutput = nullptr;
  std::atomic<float> *pMode = nullptr;
  std::atomic<float> *pQuality = nullptr;
  std::atomic<float> *pTighten = nullptr;
  std::atomic<float> *pSmooth = nullptr;
  std::atomic<float> *pPunch = nullptr;
  std::atomic<float> *pDynamics = nullptr;
  std::atomic<float> *pNetDepth = nullptr;
  std::atomic<float> *pNetSmooth = nullptr;
  std::atomic<float> *pNetSens = nullptr;
  std::atomic<float> *pHeat = nullptr;
  std::atomic<float> *pDrift = nullptr;
  std::atomic<float> *pVariance = nullptr;
  std::atomic<float> *pEntropy = nullptr;
  std::atomic<float> *pNoise = nullptr;
  std::atomic<float> *pFocus = nullptr;
  std::atomic<float> *pGroup = nullptr;
  std::atomic<float> *pRole = nullptr;
  std::atomic<float> *pNetMode = nullptr;
  std::atomic<float> *pNetReaction = nullptr;
  std::atomic<float> *pCascade = nullptr;
  std::atomic<float> *pDelta = nullptr;
};

} // namespace Cohera
