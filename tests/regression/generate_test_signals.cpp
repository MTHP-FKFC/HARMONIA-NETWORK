/**
 * Generate Reference Audio (Headless Version)
 *
 * This program generates baseline audio files by processing test signals
 * through the CURRENT version of Cohera Saturator (v1.30).
 *
 * Headless mode: No UI dependencies, just DSP processing.
 *
 * Usage:
 *   ./generate_reference_audio
 *
 * Output:
 *   tests/regression/reference_audio/ (WAV files)
 */

#include "SignalGenerator.h"
#include <iostream>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

using namespace Cohera::Testing;

int main(int argc, char *argv[]) {
  std::cout << "🎧 Cohera Saturator - Reference Audio Generator (Headless)"
            << std::endl;
  std::cout << "=========================================================="
            << std::endl;
  std::cout << std::endl;
  std::cout << "⚠️  NOTE: This is a simplified version that generates test "
               "signals only."
            << std::endl;
  std::cout
      << "    For full plugin processing, use the plugin directly in a DAW."
      << std::endl;
  std::cout << std::endl;

  // Create output directory
  juce::File outputDir("tests/regression/reference_audio");
  outputDir.createDirectory();

  // Test signals configuration
  struct TestCase {
    juce::String name;
    std::function<juce::AudioBuffer<float>()> generator;
  };

  std::vector<TestCase> testCases = {
      // 1. Sine 440Hz
      {"sine_440hz_default",
       []() { return SignalGenerator::generateSine(440.0f, 5.0f, -6.0f); }},

      // 2. Sine Sweep
      {"sine_sweep_extreme_drive",
       []() {
         return SignalGenerator::generateSineSweep(20.0f, 20000.0f, 10.0f,
                                                   -6.0f);
       }},

      // 3. White Noise
      {"white_noise_network",
       []() { return SignalGenerator::generateWhiteNoise(5.0f, -12.0f); }},

      // 4. Kick Drum
      {"kick_full_mojo",
       []() { return SignalGenerator::generateKickDrum(2.0f, -6.0f); }},

      // 5. Sine 440Hz (commercial mix placeholder)
      {"commercial_mix_default",
       []() { return SignalGenerator::generateSine(440.0f, 10.0f, -12.0f); }}};

  // Generate test signals
  int successCount = 0;
  int totalCount = static_cast<int>(testCases.size());

  for (const auto &testCase : testCases) {
    std::cout << "Generating: " << testCase.name << "..." << std::flush;

    try {
      // Generate input signal
      auto signal = testCase.generator();

      // Save to file
      juce::String outputPath =
          outputDir.getFullPathName() + "/" + testCase.name + ".wav";
      bool saved = SignalGenerator::saveToWav(signal, outputPath, 48000.0);

      if (saved) {
        std::cout << " ✅ OK" << std::endl;
        successCount++;
      } else {
        std::cout << " ❌ FAILED (save error)" << std::endl;
      }
    } catch (const std::exception &e) {
      std::cout << " ❌ FAILED (" << e.what() << ")" << std::endl;
    }
  }

  std::cout << std::endl;
  std::cout << "=========================================================="
            << std::endl;
  std::cout << "Results: " << successCount << "/" << totalCount
            << " files generated" << std::endl;

  if (successCount == totalCount) {
    std::cout << "✅ All test signals generated successfully!" << std::endl;
    std::cout << std::endl;
    std::cout << "Next steps:" << std::endl;
    std::cout << "1. Verify audio files in: " << outputDir.getFullPathName()
              << std::endl;
    std::cout << "2. Process these files through the plugin in your DAW"
              << std::endl;
    std::cout << "3. Save the processed output as reference files" << std::endl;
    std::cout << "4. Use them as baseline for regression testing" << std::endl;
    return 0;
  } else {
    std::cout << "❌ Some files failed to generate" << std::endl;
    return 1;
  }
}
