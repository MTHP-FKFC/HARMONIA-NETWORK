#pragma once

#include "../CoheraTypes.h"

namespace Cohera {

enum class NetReaction {
  Clean,      // Просто изменение громкости
  DriveBoost, // Разгон основного алгоритма
  Rectify,    // Добавление гармоник (Ghost)
  Crush       // Биткраш (Digital glitch)
};

struct ParameterSet {
  // === Main Controls ===
  float drive = 0.0f;      // 0.0 .. 100.0 (raw param)
  float mix = 1.0f;        // 0.0 .. 1.0
  float outputGain = 1.0f; // Linear gain

  // === Mode & Quality ===
  mutable SaturationMode saturationMode = SaturationMode::GoldenRatio;
  mutable QualityMode qualityMode = QualityMode::Pro;
  bool cascade = false;     // Output stage limiter
  bool deltaListen = false; // Listen to the difference signal

  // === Tone Shaping ===
  float preFilterFreq = 10.0f;
  float postFilterFreq = 22000.0f;
  float dynamics = 0.5f; // 0.0 .. 1.0

  // === Punch & Mojo ===
  float punch = 0.0f;       // -1.0 .. 1.0
  float globalHeat = 0.0f;  // 0.0 .. 1.0 (from param)
  float analogDrift = 0.0f; // 0.0 .. 1.0
  float variance = 0.0f;
  float noise = 0.0f;
  float entropy = 0.0f;
  float focus = 0.0f; // -1.0 .. 1.0 (Mid/Side focus)

  // === Network ===
  NetworkMode netMode = NetworkMode::Unmasking;
  NetReaction netReaction = NetReaction::DriveBoost; // Новый параметр
  NetworkRole netRole = NetworkRole::Listener;
  int groupId = 0;
  float netDepth = 1.0f;
  float netSmooth = 0.1f; // Normalized
  float netSens = 1.0f;

  // === Helper to Calculate Effective Drive ===
  // Переносим логику маппинга драйва сюда или в Manager,
  // чтобы DSP получал уже готовый множитель
  float getEffectiveDriveGain() const {
    // Линейное маппинг: 0% = 1.0x, 100% = 10.0x (20dB gain)
    // Теперь сатурация слышна сразу с первых процентов!
    if (drive <= 0.0f)
      return 1.0f;
    return 1.0f + (drive / 100.0f) * 9.0f; // 1.0x to 10.0x
  }

  float getSaturationBlend() const {
    if (drive < 20.0f)
      return drive / 20.0f;
    return 1.0f;
  }
};

} // namespace Cohera
