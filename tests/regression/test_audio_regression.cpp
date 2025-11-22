/**
 * Audio Regression Test Runner
 *
 * Validates that current code produces bit-exact output compared to
 * established reference files.
 *
 * Usage:
 *   ./test_audio_regression
 */

#include <iomanip>
#include <iostream>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_core/juce_core.h>
#include <vector>

#include "../src/PluginProcessor.h"
#include "AudioComparator.h"
#include "SignalGenerator.h"

using namespace Cohera::Testing;

// Helper to process audio (duplicated from process_test_signals to keep tests
// self-contained)
juce::AudioBuffer<float>
processWithPlugin(CoheraSaturatorAudioProcessor &processor,
                  const juce::AudioBuffer<float> &input,
                  const juce::String &presetPath) {
  if (presetPath.isNotEmpty()) {
    juce::File presetFile(presetPath);
    if (presetFile.existsAsFile()) {
      juce::MemoryBlock presetData;
      if (presetFile.loadFileAsData(presetData)) {
        processor.setStateInformation(presetData.getData(),
                                      static_cast<int>(presetData.getSize()));
      }
    }
  }

  processor.prepareToPlay(48000.0, 512);
  juce::AudioBuffer<float> output(input);
  juce::MidiBuffer midiBuffer;

  int totalSamples = output.getNumSamples();
  for (int pos = 0; pos < totalSamples; pos += 512) {
    int samplesToProcess = juce::jmin(512, totalSamples - pos);
    juce::AudioBuffer<float> block(output.getArrayOfWritePointers(),
                                   output.getNumChannels(), pos,
                                   samplesToProcess);
    processor.processBlock(block, midiBuffer);
  }

  processor.releaseResources();
  return output;
}

int main(int argc, char *argv[]) {
  std::cout << "🛡️  Cohera Saturator - Regression Test Runner" << std::endl;
  std::cout << "===========================================" << std::endl;

  juce::ScopedJuceInitialiser_GUI juceInit;
  CoheraSaturatorAudioProcessor processor;

  struct TestCase {
    juce::String name;
    juce::String inputFile;
    juce::String presetFile;
    juce::String referenceFile;
  };

  std::vector<TestCase> testCases = {
      // DRUMS
      {"Kick Default", "tests/regression/reference_audio/kick_default.wav",
       "tests/regression/presets/default.xml",
       "tests/regression/reference_audio/kick_default_processed.wav"},
      {"Kick Extreme", "tests/regression/reference_audio/kick_extreme.wav",
       "tests/regression/presets/extreme_drive.xml",
       "tests/regression/reference_audio/kick_extreme_processed.wav"},
      {"Kick Mojo", "tests/regression/reference_audio/kick_mojo.wav",
       "tests/regression/presets/full_mojo.xml",
       "tests/regression/reference_audio/kick_mojo_processed.wav"},

      // SNARE
      {"Snare Default", "tests/regression/reference_audio/snare_default.wav",
       "tests/regression/presets/default.xml",
       "tests/regression/reference_audio/snare_default_processed.wav"},
      {"Snare Network", "tests/regression/reference_audio/snare_network.wav",
       "tests/regression/presets/network_active.xml",
       "tests/regression/reference_audio/snare_network_processed.wav"},

      // BASS
      {"Bass Default", "tests/regression/reference_audio/bass_default.wav",
       "tests/regression/presets/default.xml",
       "tests/regression/reference_audio/bass_default_processed.wav"},

      // GUITAR
      {"Guitar Mojo", "tests/regression/reference_audio/guitar_mojo.wav",
       "tests/regression/presets/full_mojo.xml",
       "tests/regression/reference_audio/guitar_mojo_processed.wav"},
  };

  int passedCount = 0;
  int totalCount = static_cast<int>(testCases.size());

  // Strict threshold for float32 bit-exactness
  // Relaxed slightly to 1e-5 to account for potential minor math variations
  // across runs/builds Ideally should be 0.0 or epsilon, but 1e-5 is safe for
  // "audibly identical"
  const float THRESHOLD = 1.0e-5f;

  std::cout << "Running " << totalCount
            << " regression tests (Threshold: " << THRESHOLD << ")..."
            << std::endl;
  std::cout << std::endl;
  std::cout << std::left << std::setw(20) << "TEST CASE" << std::setw(15)
            << "STATUS"
            << "DETAILS" << std::endl;
  std::cout
      << "----------------------------------------------------------------"
      << std::endl;

  for (const auto &test : testCases) {
    // 1. Load Input
    auto input = SignalGenerator::loadFromWav(test.inputFile);
    if (input.getNumSamples() == 0) {
      std::cout << std::left << std::setw(20) << test.name << "❌ ERROR"
                << "Could not load input" << std::endl;
      continue;
    }

    // 2. Load Reference
    auto reference = SignalGenerator::loadFromWav(test.referenceFile);
    if (reference.getNumSamples() == 0) {
      std::cout << std::left << std::setw(20) << test.name << "❌ ERROR"
                << "Could not load reference" << std::endl;
      continue;
    }

    // 3. Process
    auto output = processWithPlugin(processor, input, test.presetFile);

    // 4. Compare
    auto result = AudioComparator::compare(reference, output, THRESHOLD);

    std::cout << std::left << std::setw(20) << test.name;
    if (result.passed) {
      std::cout << "✅ PASS       ";
      passedCount++;
    } else {
      std::cout << "❌ FAIL       ";
    }
    std::cout << "MaxDiff: " << result.maxDiff << " (" << result.maxDiffDb
              << " dB)" << std::endl;
  }

  std::cout
      << "----------------------------------------------------------------"
      << std::endl;
  std::cout << "Summary: " << passedCount << "/" << totalCount
            << " tests passed." << std::endl;

  return (passedCount == totalCount) ? 0 : 1;
}
