#pragma once

#include "../CoheraTypes.h"

namespace Cohera {

struct ParameterSet
{
    // === Main Controls ===
    float drive = 0.0f;        // 0.0 .. 100.0 (raw param)
    float mix = 1.0f;          // 0.0 .. 1.0
    float outputGain = 1.0f;   // Linear gain

    // === Mode & Quality ===
    mutable MathMode mathMode = MathMode::GoldenRatio;
    mutable QualityMode qualityMode = QualityMode::Pro;

    // === Tone Shaping ===
    float preFilterFreq = 10.0f;
    float postFilterFreq = 22000.0f;
    float dynamics = 0.5f;     // 0.0 .. 1.0

    // === Punch & Mojo ===
    float punch = 0.0f;        // -1.0 .. 1.0
    float globalHeat = 0.0f;   // 0.0 .. 1.0 (from param)
    float analogDrift = 0.0f;  // 0.0 .. 1.0
    float variance = 0.0f;
    float noise = 0.0f;
    float entropy = 0.0f;

    // === Network ===
    NetworkMode netMode = NetworkMode::Unmasking;
    NetworkRole netRole = NetworkRole::Listener;
    int groupId = 0;
    float netDepth = 1.0f;
    float netSmooth = 0.1f; // Normalized
    float netSens = 1.0f;

    // === Helper to Calculate Effective Drive ===
    // Переносим логику маппинга драйва сюда или в Manager,
    // чтобы DSP получал уже готовый множитель
    float getEffectiveDriveGain() const
    {
        // Пример простой формулы (как было в вашем коде)
        if (drive < 20.0f) return 1.0f;
        float boost = (drive - 20.0f) / 80.0f;
        return 1.0f + (boost * 9.0f); // до +20dB (x10)
    }

    float getSaturationBlend() const
    {
        if (drive < 20.0f) return drive / 20.0f;
        return 1.0f;
    }
};

} // namespace Cohera
