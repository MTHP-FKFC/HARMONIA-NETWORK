/**
 * Process Test Signals Through Plugin
 *
 * This program loads test signals and processes them through
 * Cohera Saturator plugin to create reference audio files.
 *
 * Usage:
 *   ./process_test_signals
 *
 * Input:  tests/regression/reference_audio/*.wav (test signals)
 * Output: tests/regression/reference_audio/*_processed.wav
 */

#include "../src/PluginProcessor.h"
#include "SignalGenerator.h"
#include <iostream>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>

using namespace Cohera::Testing;

/**
 * Process audio buffer through plugin with specific preset
 */
juce::AudioBuffer<float>
processWithPlugin(CoheraSaturatorAudioProcessor &processor,
                  const juce::AudioBuffer<float> &input,
                  const juce::String &presetPath) {
  // Load preset if provided
  if (presetPath.isNotEmpty()) {
    juce::File presetFile(presetPath);
    if (presetFile.existsAsFile()) {
      juce::MemoryBlock presetData;
      if (presetFile.loadFileAsData(presetData)) {
        processor.setStateInformation(presetData.getData(),
                                      static_cast<int>(presetData.getSize()));
        std::cout << "  Loaded preset: " << presetFile.getFileName()
                  << std::endl;
      }
    } else {
      std::cout << "  Using default preset (file not found)" << std::endl;
    }
  }

  // Prepare processor
  processor.prepareToPlay(48000.0, 512);

  // Create output buffer (copy input)
  juce::AudioBuffer<float> output(input);

  // Process in blocks of 512 samples
  const int blockSize = 512;
  juce::MidiBuffer midiBuffer;

  int totalSamples = output.getNumSamples();
  int processedSamples = 0;

  for (int pos = 0; pos < totalSamples; pos += blockSize) {
    int samplesToProcess = juce::jmin(blockSize, totalSamples - pos);

    // Create view of current block
    juce::AudioBuffer<float> block(output.getArrayOfWritePointers(),
                                   output.getNumChannels(), pos,
                                   samplesToProcess);

    // Process block
    processor.processBlock(block, midiBuffer);

    processedSamples += samplesToProcess;

    // Progress indicator (every 10%)
    int progress = (processedSamples * 100) / totalSamples;
    if (progress % 10 == 0 && pos > 0) {
      std::cout << "  Progress: " << progress << "%" << std::flush << "\r";
    }
  }

  std::cout << "  Progress: 100%    " << std::endl;

  processor.releaseResources();
  return output;
}

int main(int argc, char *argv[]) {
  std::cout << "🎛️  Cohera Saturator - Test Signal Processor" << std::endl;
  std::cout << "=============================================" << std::endl;
  std::cout << std::endl;

  // Test cases: input file -> preset file
  struct TestCase {
    juce::String inputFile;
    juce::String presetFile;
    juce::String outputFile;
  };

  std::vector<TestCase> testCases = {
      {"tests/regression/reference_audio/sine_440hz_default.wav",
       "tests/regression/presets/default.xml",
       "tests/regression/reference_audio/sine_440hz_default_processed.wav"},
      {"tests/regression/reference_audio/sine_sweep_extreme_drive.wav",
       "tests/regression/presets/extreme_drive.xml",
       "tests/regression/reference_audio/"
       "sine_sweep_extreme_drive_processed.wav"},
      {"tests/regression/reference_audio/white_noise_network.wav",
       "tests/regression/presets/network_active.xml",
       "tests/regression/reference_audio/white_noise_network_processed.wav"},
      {"tests/regression/reference_audio/kick_full_mojo.wav",
       "tests/regression/presets/full_mojo.xml",
       "tests/regression/reference_audio/kick_full_mojo_processed.wav"},
      {"tests/regression/reference_audio/commercial_mix_default.wav",
       "tests/regression/presets/default.xml",
       "tests/regression/reference_audio/"
       "commercial_mix_default_processed.wav"}};

  // Create processor instance
  CoheraSaturatorAudioProcessor processor;

  int successCount = 0;
  int totalCount = static_cast<int>(testCases.size());

  for (const auto &testCase : testCases) {
    std::cout << "Processing: " << juce::File(testCase.inputFile).getFileName()
              << std::endl;

    try {
      // Load input audio
      auto inputSignal = SignalGenerator::loadFromWav(testCase.inputFile);

      if (inputSignal.getNumSamples() == 0) {
        std::cout << "  ❌ FAILED (could not load input file)" << std::endl;
        continue;
      }

      std::cout << "  Loaded: " << inputSignal.getNumSamples() << " samples, "
                << inputSignal.getNumChannels() << " channels" << std::endl;

      // Process through plugin
      auto outputSignal =
          processWithPlugin(processor, inputSignal, testCase.presetFile);

      // Save to file
      bool saved = SignalGenerator::saveToWav(outputSignal, testCase.outputFile,
                                              48000.0);

      if (saved) {
        std::cout << "  ✅ Saved: "
                  << juce::File(testCase.outputFile).getFileName() << std::endl;
        successCount++;
      } else {
        std::cout << "  ❌ FAILED (save error)" << std::endl;
      }
    } catch (const std::exception &e) {
      std::cout << "  ❌ FAILED (" << e.what() << ")" << std::endl;
    }

    std::cout << std::endl;
  }

  std::cout << "=============================================" << std::endl;
  std::cout << "Results: " << successCount << "/" << totalCount
            << " files processed" << std::endl;

  if (successCount == totalCount) {
    std::cout << "✅ All reference files generated successfully!" << std::endl;
    std::cout << std::endl;
    std::cout << "Next steps:" << std::endl;
    std::cout << "1. Verify processed files sound correct" << std::endl;
    std::cout << "2. Commit reference files to git" << std::endl;
    std::cout << "3. Create regression test runner" << std::endl;
    return 0;
  } else {
    std::cout << "❌ Some files failed to process" << std::endl;
    return 1;
  }
}
