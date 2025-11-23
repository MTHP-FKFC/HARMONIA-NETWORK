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

#include "../../src/PluginProcessor.h"
#include "SignalGenerator.h"
#include <iostream>
#include <fstream>
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
        // std::cout << "  Loaded preset: " << presetFile.getFileName() <<
        // std::endl;
      }
    } else {
      // std::cout << "  Using default preset (file not found)" << std::endl;
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

  for (int pos = 0; pos < totalSamples; pos += blockSize) {
    int samplesToProcess = juce::jmin(blockSize, totalSamples - pos);

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

/**
 * Run thermal dynamics analysis - generates CSV data for visualization
 */
void runThermalAnalysis() {
    std::cout << "🔥 Running Thermal Dynamics Analysis..." << std::endl;
    
    CoheraSaturatorAudioProcessor plugin;
    
    // Setup plugin for maximum thermal effect
    plugin.prepareToPlay(44100.0, 512);
    
    std::ofstream csvFile("thermal_debug.csv");
    csvFile << "Time,Input,Output,Temperature\n";
    
    float sampleRate = 44100.0f;
    float freq = 100.0f; // 100 Hz test tone
    
    // Simulate 5 seconds of operation
    for (int i = 0; i < sampleRate * 5; ++i) {
        // Generate sine wave
        float input = std::sin(2.0f * 3.14159f * freq * (float)i / sampleRate) * 0.8f;
        
        // Create buffer
        juce::AudioBuffer<float> buffer(2, 1);
        buffer.setSample(0, 0, input);
        buffer.setSample(1, 0, input);
        
        // Process
        juce::MidiBuffer midi;
        plugin.processBlock(buffer, midi);
        
        float output = buffer.getSample(0, 0);
        
        // Get temperature - ЗАГЛУШКА, нужно заменить на реальный геттер!
        // TODO: Replace with actual thermal model temperature
        float thermalGain = std::abs(input) * 0.5f; // Simulated heating
        static float accumulatedHeat = 0.0f;
        accumulatedHeat += thermalGain * (1.0f / sampleRate);
        accumulatedHeat *= 0.999f; // Cooling factor
        float temp = accumulatedHeat * 100.0f; // Scale to 0-100 range
        
        csvFile << (float)i/sampleRate << "," << input << "," << output << "," << temp << "\n";
        
        // Print progress every second
        if (i % (int)sampleRate == 0) {
            std::cout << "  Processing second " << (i/(int)sampleRate + 1) << "/5..." << std::endl;
        }
    }
    
    csvFile.close();
    std::cout << "✅ Thermal data dumped to thermal_debug.csv" << std::endl;
}

int main(int argc, char *argv[]) {
  std::cout << "🎛️  Cohera Saturator - Headless DSP Processor" << std::endl;
  std::cout << "=============================================" << std::endl;
  std::cout << std::endl;

  // Initialize JUCE (needed for some modules, even headless)
  juce::ScopedJuceInitialiser_GUI juceInit;

  // Test cases: input file -> preset file
  struct TestCase {
    juce::String inputFile;
    juce::String presetFile;
    juce::String outputFile;
  };

  std::vector<TestCase> testCases = {
      // === DRUMS ===
      {"tests/regression/reference_audio/kick_default.wav",
       "tests/regression/presets/default.xml",
       "tests/regression/reference_audio/kick_default_processed.wav"},
      {"tests/regression/reference_audio/kick_extreme.wav",
       "tests/regression/presets/extreme_drive.xml",
       "tests/regression/reference_audio/kick_extreme_processed.wav"},
      {"tests/regression/reference_audio/kick_mojo.wav",
       "tests/regression/presets/full_mojo.xml",
       "tests/regression/reference_audio/kick_mojo_processed.wav"},

      {"tests/regression/reference_audio/snare_default.wav",
       "tests/regression/presets/default.xml",
       "tests/regression/reference_audio/snare_default_processed.wav"},
      {"tests/regression/reference_audio/snare_extreme.wav",
       "tests/regression/presets/extreme_drive.xml",
       "tests/regression/reference_audio/snare_extreme_processed.wav"},
      {"tests/regression/reference_audio/snare_network.wav",
       "tests/regression/presets/network_active.xml",
       "tests/regression/reference_audio/snare_network_processed.wav"},

      {"tests/regression/reference_audio/hihat_default.wav",
       "tests/regression/presets/default.xml",
       "tests/regression/reference_audio/hihat_default_processed.wav"},
      {"tests/regression/reference_audio/hihat_extreme.wav",
       "tests/regression/presets/extreme_drive.xml",
       "tests/regression/reference_audio/hihat_extreme_processed.wav"},
      {"tests/regression/reference_audio/hihat_mojo.wav",
       "tests/regression/presets/full_mojo.xml",
       "tests/regression/reference_audio/hihat_mojo_processed.wav"},

      // === BASS ===
      {"tests/regression/reference_audio/bass_default.wav",
       "tests/regression/presets/default.xml",
       "tests/regression/reference_audio/bass_default_processed.wav"},
      {"tests/regression/reference_audio/bass_extreme.wav",
       "tests/regression/presets/extreme_drive.xml",
       "tests/regression/reference_audio/bass_extreme_processed.wav"},
      {"tests/regression/reference_audio/bass_network.wav",
       "tests/regression/presets/network_active.xml",
       "tests/regression/reference_audio/bass_network_processed.wav"},

      // === GUITAR ===
      {"tests/regression/reference_audio/guitar_default.wav",
       "tests/regression/presets/default.xml",
       "tests/regression/reference_audio/guitar_default_processed.wav"},
      {"tests/regression/reference_audio/guitar_extreme.wav",
       "tests/regression/presets/extreme_drive.xml",
       "tests/regression/reference_audio/guitar_extreme_processed.wav"},
      {"tests/regression/reference_audio/guitar_mojo.wav",
       "tests/regression/presets/full_mojo.xml",
       "tests/regression/reference_audio/guitar_mojo_processed.wav"},
  };

  // Create processor instance
  CoheraSaturatorAudioProcessor processor;

  int successCount = 0;
  int totalCount = static_cast<int>(testCases.size());

  std::cout << "Processing " << totalCount << " files..." << std::endl;

  for (const auto &testCase : testCases) {
    std::cout << "Processing: " << juce::File(testCase.inputFile).getFileName()
              << "..." << std::flush;

    try {
      // Load input audio
      auto inputSignal = SignalGenerator::loadFromWav(testCase.inputFile);

      if (inputSignal.getNumSamples() == 0) {
        std::cout << " ❌ FAILED (load error)" << std::endl;
        continue;
      }

      // Process through plugin
      auto outputSignal =
          processWithPlugin(processor, inputSignal, testCase.presetFile);

      // Save to file
      bool saved = SignalGenerator::saveToWav(outputSignal, testCase.outputFile,
                                              48000.0);

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
  std::cout << "=============================================" << std::endl;
  std::cout << "Results: " << successCount << "/" << totalCount
            << " files processed" << std::endl;

  if (successCount == totalCount) {
    std::cout << "✅ All reference files generated successfully!" << std::endl;
    return 0;
  } else {
    std::cout << "❌ Some files failed to process" << std::endl;
    return 1;
  }
}
