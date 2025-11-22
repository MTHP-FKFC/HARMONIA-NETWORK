/**
 * Generate Realistic Instrument Test Signals
 *
 * Generates professional-grade test signals for regression testing:
 * - Drums: Kick, Snare, Hi-hat
 * - Bass: Low-frequency sine wave
 * - Guitar: Pink noise (realistic texture)
 *
 * Each instrument × 3 presets = 15 test cases total
 */

#include "SignalGenerator.h"
#include <iostream>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

using namespace Cohera::Testing;

int main(int argc, char *argv[]) {
  std::cout << "🎸 Cohera Saturator - Realistic Instrument Signal Generator"
            << std::endl;
  std::cout << "==========================================================="
            << std::endl;
  std::cout << std::endl;

  // Create output directory
  juce::File outputDir("tests/regression/reference_audio");
  outputDir.createDirectory();

  // Test cases: instrument + preset
  struct TestCase {
    juce::String name;
    std::function<juce::AudioBuffer<float>()> generator;
  };

  std::vector<TestCase> testCases = {
      // === DRUMS ===
      // Kick (3 presets)
      {"kick_default",
       []() { return SignalGenerator::generateKickDrum(2.0f, -6.0f); }},
      {"kick_extreme",
       []() { return SignalGenerator::generateKickDrum(2.0f, -3.0f); }},
      {"kick_mojo",
       []() { return SignalGenerator::generateKickDrum(2.0f, -6.0f); }},

      // Snare (3 presets)
      {"snare_default",
       []() { return SignalGenerator::generateSnareDrum(1.0f, -6.0f); }},
      {"snare_extreme",
       []() { return SignalGenerator::generateSnareDrum(1.0f, -3.0f); }},
      {"snare_network",
       []() { return SignalGenerator::generateSnareDrum(1.0f, -6.0f); }},

      // Hi-hat (3 presets)
      {"hihat_default",
       []() { return SignalGenerator::generateHiHat(0.5f, -12.0f); }},
      {"hihat_extreme",
       []() { return SignalGenerator::generateHiHat(0.5f, -9.0f); }},
      {"hihat_mojo",
       []() { return SignalGenerator::generateHiHat(0.5f, -12.0f); }},

      // === BASS ===
      // Bass (3 presets) - A1 (55Hz)
      {"bass_default",
       []() { return SignalGenerator::generateBass(55.0f, 4.0f, -6.0f); }},
      {"bass_extreme",
       []() { return SignalGenerator::generateBass(55.0f, 4.0f, -3.0f); }},
      {"bass_network",
       []() { return SignalGenerator::generateBass(55.0f, 4.0f, -6.0f); }},

      // === GUITAR ===
      // Guitar (pink noise, 3 presets)
      {"guitar_default",
       []() { return SignalGenerator::generatePinkNoise(4.0f, -12.0f); }},
      {"guitar_extreme",
       []() { return SignalGenerator::generatePinkNoise(4.0f, -9.0f); }},
      {"guitar_mojo",
       []() { return SignalGenerator::generatePinkNoise(4.0f, -12.0f); }},
  };

  int successCount = 0;
  int totalCount = static_cast<int>(testCases.size());

  std::cout << "Generating " << totalCount << " instrument signals..."
            << std::endl;
  std::cout << std::endl;

  for (const auto &testCase : testCases) {
    std::cout << "Generating: " << testCase.name << "..." << std::flush;

    try {
      // Generate signal
      auto signal = testCase.generator();

      // Save to file
      juce::String outputPath =
          outputDir.getFullPathName() + "/" + testCase.name + ".wav";
      bool saved = SignalGenerator::saveToWav(signal, outputPath, 48000.0);

      if (saved) {
        std::cout << " ✅ OK (" << signal.getNumSamples() << " samples)"
                  << std::endl;
        successCount++;
      } else {
        std::cout << " ❌ FAILED (save error)" << std::endl;
      }
    } catch (const std::exception &e) {
      std::cout << " ❌ FAILED (" << e.what() << ")" << std::endl;
    }
  }

  std::cout << std::endl;
  std::cout << "==========================================================="
            << std::endl;
  std::cout << "Results: " << successCount << "/" << totalCount
            << " files generated" << std::endl;

  if (successCount == totalCount) {
    std::cout << "✅ All instrument signals generated successfully!"
              << std::endl;
    std::cout << std::endl;
    std::cout << "Generated instruments:" << std::endl;
    std::cout << "  🥁 Drums: Kick (3), Snare (3), Hi-hat (3)" << std::endl;
    std::cout << "  🎸 Bass: Low sine wave (3 presets)" << std::endl;
    std::cout << "  🎵 Guitar: Pink noise texture (3 presets)" << std::endl;
    std::cout << std::endl;
    std::cout << "Total: 15 realistic instrument test signals" << std::endl;
    std::cout << std::endl;
    std::cout << "Next: Process these through plugin with corresponding presets"
              << std::endl;
    return 0;
  } else {
    std::cout << "❌ Some files failed to generate" << std::endl;
    return 1;
  }
}
