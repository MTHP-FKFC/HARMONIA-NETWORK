#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ParameterSet.h"

using namespace Cohera;

namespace Cohera {

class ParameterManager
{
public:
    ParameterManager(juce::AudioProcessorValueTreeState& apvtsRef)
        : apvts(apvtsRef)
    {
        // Кэшируем указатели на параметры для скорости (вместо getRawParameterValue по строке)
        pDrive      = apvts.getRawParameterValue("drive_master");
        pMix        = apvts.getRawParameterValue("mix");
        pOutput     = apvts.getRawParameterValue("output_gain");
        pMode       = apvts.getRawParameterValue("math_mode");
        pQuality    = apvts.getRawParameterValue("quality");
        pTighten    = apvts.getRawParameterValue("tone_tighten");
        pSmooth     = apvts.getRawParameterValue("tone_smooth");
        pPunch      = apvts.getRawParameterValue("punch");
        pDynamics   = apvts.getRawParameterValue("dynamics");
        pNetDepth   = apvts.getRawParameterValue("net_depth");
        pNetSmooth  = apvts.getRawParameterValue("net_smooth");
        pNetSens    = apvts.getRawParameterValue("net_sens");
        pHeat       = apvts.getRawParameterValue("heat");
        pDrift      = apvts.getRawParameterValue("analog_drift");
        pVariance   = apvts.getRawParameterValue("variance");
        pEntropy    = apvts.getRawParameterValue("entropy");
        pNoise      = apvts.getRawParameterValue("noise");
        pFocus      = apvts.getRawParameterValue("focus");
        pGroup      = apvts.getRawParameterValue("group_id");
        pRole       = apvts.getRawParameterValue("role");
        pNetMode    = apvts.getRawParameterValue("mode");
        pCascade    = apvts.getRawParameterValue("cascade");
        // ... добавьте остальные по аналогии
    }

    // Вызывается в начале processBlock
    ParameterSet getCurrentParams() const
    {
        ParameterSet params;

        // Читаем атомики
        params.drive = pDrive->load();
        params.mix = pMix->load() / 100.0f;
        params.outputGain = juce::Decibels::decibelsToGain(pOutput->load());

        params.saturationMode = static_cast<SaturationMode>((int)pMode->load());
        params.qualityMode = static_cast<QualityMode>((int)pQuality->load());
        params.cascade = pCascade->load() > 0.5f;

        params.preFilterFreq = pTighten->load();
        params.postFilterFreq = pSmooth->load();
        params.punch = pPunch->load() / 100.0f;
        params.dynamics = pDynamics->load() / 100.0f;

        params.netDepth = pNetDepth->load() / 100.0f;
        params.netSmooth = pNetSmooth->load() / 100.0f;
        params.netSens = pNetSens->load() / 100.0f;

        params.globalHeat = pHeat->load() / 100.0f;
        params.analogDrift = pDrift->load() / 100.0f;
        params.variance = pVariance->load() / 100.0f;
        params.entropy = pEntropy->load() / 100.0f;
        params.noise = pNoise->load() / 100.0f;

        params.groupId = (int)pGroup->load();
        params.netRole = static_cast<NetworkRole>((int)pRole->load());
        params.netMode = static_cast<NetworkMode>((int)pNetMode->load());

        return params;
    }

private:
    juce::AudioProcessorValueTreeState& apvts;

    // Кэшированные указатели (std::atomic<float>*)
    std::atomic<float>* pDrive = nullptr;
    std::atomic<float>* pMix = nullptr;
    std::atomic<float>* pOutput = nullptr;
    std::atomic<float>* pMode = nullptr;
    std::atomic<float>* pQuality = nullptr;
    std::atomic<float>* pTighten = nullptr;
    std::atomic<float>* pSmooth = nullptr;
    std::atomic<float>* pPunch = nullptr;
    std::atomic<float>* pDynamics = nullptr;
    std::atomic<float>* pNetDepth = nullptr;
    std::atomic<float>* pNetSmooth = nullptr;
    std::atomic<float>* pNetSens = nullptr;
    std::atomic<float>* pHeat = nullptr;
    std::atomic<float>* pDrift = nullptr;
    std::atomic<float>* pVariance = nullptr;
    std::atomic<float>* pEntropy = nullptr;
    std::atomic<float>* pNoise = nullptr;
    std::atomic<float>* pFocus = nullptr;
    std::atomic<float>* pGroup = nullptr;
    std::atomic<float>* pRole = nullptr;
    std::atomic<float>* pNetMode = nullptr;
    std::atomic<float>* pCascade = nullptr;
};

} // namespace Cohera
