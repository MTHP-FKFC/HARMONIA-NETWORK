/**
 * Generate Reference Audio
 *
 * This program generates baseline audio files by processing test signals
 * through the CURRENT version of Cohera Saturator (v1.30).
 *
 * These files will be used as "golden reference" for regression testing.
 *
 * Usage:
 *   ./generate_reference_audio
 *
 * Output:
 *   tests/regression/reference_audio/ (WAV files)
 */

#include "../src/PluginProcessor.h"
#include "SignalGenerator.h"
#include <JuceHeader.h>
#include <iostream>

using namespace Cohera::Testing;

// Helper: Process audio through plugin with preset
juce::AudioBuffer<float>
processWithPreset(CoheraSaturatorAudioProcessor &processor,
                  const juce::AudioBuffer<float> &input,
                  const juce::String &presetPath) {
  // Load preset if provided
  if (presetPath.isNotEmpty()) {
    juce::File presetFile(presetPath);
    if (presetFile.existsAsFile()) {
      juce::MemoryBlock presetData;
      presetFile.loadFileAsData(presetData);
      processor.setStateInformation(presetData.getData(),
                                    static_cast<int>(presetData.getSize()));
    }
  }

  // Prepare processor
  processor.prepareToPlay(48000.0, 512);

  // Create output buffer (copy input)
  juce::AudioBuffer<float> output(input);

  // Process in blocks of 512 samples
  const int blockSize = 512;
  juce::MidiBuffer midiBuffer;

  for (int pos = 0; pos < output.getNumSamples(); pos += blockSize) {
    int samplesToProcess = juce::jmin(blockSize, output.getNumSamples() - pos);

    // Create view of current block
    juce::AudioBuffer<float> block(output.getArrayOfWritePointers(),
                                   output.getNumChannels(), pos,
                                   samplesToProcess);

    // Process block
    processor.processBlock(block, midiBuffer);
  }

  processor.releaseResources();
  return output;
}

int main(int argc, char *argv[]) {
  std::cout << "🎧 Cohera Saturator - Reference Audio Generator" << std::endl;
  std::cout << "=================================================" << std::endl;
  std::cout << std::endl;

  // Initialize JUCE
  juce::ScopedJuceInitialiser_GUI juceInit;

  // Create output directory
  juce::File outputDir("tests/regression/reference_audio");
  outputDir.createDirectory();

  // Create processor instance
  CoheraSaturatorAudioProcessor processor;

  // Test signals configuration
  struct TestCase {
    juce::String name;
    std::function<juce::AudioBuffer<float>()> generator;
    juce::String presetPath;
  };

  std::vector<TestCase> testCases = {
      // 1. Sine 440Hz with Default preset
      {
          "sine_440hz_default",
          []() { return SignalGenerator::generateSine(440.0f, 5.0f, -6.0f); },
          "" // Default preset (no file needed)
      },

      // 2. Sine Sweep with Extreme Drive
      {"sine_sweep_extreme_drive",
       []() {
         return SignalGenerator::generateSineSweep(20.0f, 20000.0f, 10.0f,
                                                   -6.0f);
       },
       "tests/regression/presets/extreme_drive.xml"},

      // 3. White Noise with Network Active
      {"white_noise_network",
       []() { return SignalGenerator::generateWhiteNoise(5.0f, -12.0f); },
       "tests/regression/presets/network_active.xml"},

      // 4. Kick Drum with Full Mojo
      {"kick_full_mojo",
       []() { return SignalGenerator::generateKickDrum(2.0f, -6.0f); },
       "tests/regression/presets/full_mojo.xml"},

      // 5. Sine 440Hz with Default (for commercial mix, we'll use sine for now)
      // TODO: Replace with actual commercial mix sample
      {"commercial_mix_default",
       []() { return SignalGenerator::generateSine(440.0f, 10.0f, -12.0f); },
       ""}};

  // Generate reference audio for each test case
  int successCount = 0;
  int totalCount = static_cast<int>(testCases.size());

  for (const auto &testCase : testCases) {
    std::cout << "Generating: " << testCase.name << "..." << std::flush;

    try {
      // Generate input signal
      auto inputSignal = testCase.generator();

      // Process through plugin
      auto outputSignal =
          processWithPreset(processor, inputSignal, testCase.presetPath);

      // Save to file
      juce::String outputPath =
          outputDir.getFullPathName() + "/" + testCase.name + ".wav";
      bool saved =
          SignalGenerator::saveToWav(outputSignal, outputPath, 48000.0);

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
  std::cout << "=================================================" << std::endl;
  std::cout << "Results: " << successCount << "/" << totalCount
            << " files generated" << std::endl;

  if (successCount == totalCount) {
    std::cout << "✅ All reference files generated successfully!" << std::endl;
    std::cout << std::endl;
    std::cout << "Next steps:" << std::endl;
    std::cout << "1. Verify audio files in: " << outputDir.getFullPathName()
              << std::endl;
    std::cout << "2. Listen to files to ensure they sound correct" << std::endl;
    std::cout << "3. Commit these files to version control" << std::endl;
    std::cout << "4. Use them as baseline for regression testing" << std::endl;
    return 0;
  } else {
    std::cout << "❌ Some files failed to generate" << std::endl;
    return 1;
  }
}
