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

        params.mathMode = static_cast<MathMode>((int)pMode->load());
        params.qualityMode = static_cast<QualityMode>((int)pQuality->load());

        params.preFilterFreq = pTighten->load();
        params.postFilterFreq = pSmooth->load();
        params.punch = pPunch->load() / 100.0f;

        // TODO: Добавить чтение остальных параметров (Network, Analog, etc.)
        // Пока оставим дефолтные из struct ParameterSet

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
};

} // namespace Cohera
