/*
  Cohera Saturator - Stress Testing Suite
  Tests edge cases, rapid parameter changes, and extreme scenarios
*/

#include "../src/JuceHeader.h"
#include "../src/PluginProcessor.h"

#include <chrono>
#include <cmath>
#include <iostream>

#define REQUIRE(x)                                                             \
  if (!(x)) {                                                                  \
    std::cerr << "FAIL: " << #x << " at line " << __LINE__ << std::endl;       \
    return false;                                                              \
  }

class StressTester {
public:
  static bool testEdgeCaseParameters() {
    std::cout << "\n🔬 Testing Edge Case Parameters..." << std::endl;

    CoheraSaturatorAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    auto &apvts = processor.getAPVTS();
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    // Test 1: Drive = 0 (minimum)
    std::cout << "  Testing drive = 0..." << std::flush;
    if (auto *param = apvts.getParameter("drive_master")) {
      param->setValueNotifyingHost(0.0f);
    }
    buffer.clear();
    for (int i = 0; i < 512; ++i) {
      buffer.setSample(0, i, std::sin(2.0 * M_PI * 440.0 * i / 44100.0));
      buffer.setSample(1, i, std::sin(2.0 * M_PI * 440.0 * i / 44100.0));
    }
    processor.processBlock(buffer, midi);
    REQUIRE(!std::isnan(buffer.getSample(0, 0)));
    REQUIRE(!std::isinf(buffer.getSample(0, 0)));
    std::cout << " ✅" << std::endl;

    // Test 2: Drive = 1.0 (maximum)
    std::cout << "  Testing drive = 1.0..." << std::flush;
    if (auto *param = apvts.getParameter("drive_master")) {
      param->setValueNotifyingHost(1.0f);
    }
    buffer.clear();
    for (int i = 0; i < 512; ++i) {
      buffer.setSample(0, i, std::sin(2.0 * M_PI * 440.0 * i / 44100.0));
      buffer.setSample(1, i, std::sin(2.0 * M_PI * 440.0 * i / 44100.0));
    }
    processor.processBlock(buffer, midi);
    REQUIRE(!std::isnan(buffer.getSample(0, 0)));
    REQUIRE(!std::isinf(buffer.getSample(0, 0)));
    std::cout << " ✅" << std::endl;

    // Test 3: Mix = 0 (all dry)
    std::cout << "  Testing mix = 0 (all dry)..." << std::flush;
    if (auto *param = apvts.getParameter("mix")) {
      param->setValueNotifyingHost(0.0f);
    }
    buffer.clear();
    for (int i = 0; i < 512; ++i) {
      buffer.setSample(0, i, 1.0f);
      buffer.setSample(1, i, 1.0f);
    }
    processor.processBlock(buffer, midi);
    // Should be mostly dry
    REQUIRE(std::abs(buffer.getSample(0, 256)) > 0.5f);
    std::cout << " ✅" << std::endl;

    // Test 4: Mix = 1.0 (all wet)
    std::cout << "  Testing mix = 1.0 (all wet)..." << std::flush;
    if (auto *param = apvts.getParameter("mix")) {
      param->setValueNotifyingHost(1.0f);
    }
    processor.processBlock(buffer, midi);
    REQUIRE(!std::isnan(buffer.getSample(0, 0)));
    std::cout << " ✅" << std::endl;

    // Test 5: All gains at 0dB
    std::cout << "  Testing all gains at 0dB..." << std::flush;
    if (auto *param = apvts.getParameter("gain")) {
      param->setValueNotifyingHost(0.5f); // 0dB is usually at 0.5 normalized
    }
    processor.processBlock(buffer, midi);
    REQUIRE(!std::isnan(buffer.getSample(0, 0)));
    std::cout << " ✅" << std::endl;

    return true;
  }

  static bool testRapidParameterChanges() {
    std::cout << "\n⚡ Testing Rapid Parameter Changes..." << std::endl;

    CoheraSaturatorAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    auto &apvts = processor.getAPVTS();
    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    std::cout << "  Rapid drive changes (1000 iterations)..." << std::flush;
    for (int iter = 0; iter < 1000; ++iter) {
      // Rapidly toggle drive
      float value = (iter % 2 == 0) ? 0.0f : 1.0f;
      if (auto *param = apvts.getParameter("drive_master")) {
        param->setValueNotifyingHost(value);
      }

      buffer.clear();
      for (int i = 0; i < 512; ++i) {
        buffer.setSample(0, i, std::sin(2.0 * M_PI * 440.0 * i / 44100.0));
        buffer.setSample(1, i, std::sin(2.0 * M_PI * 440.0 * i / 44100.0));
      }

      processor.processBlock(buffer, midi);

      REQUIRE(!std::isnan(buffer.getSample(0, 256)));
      REQUIRE(!std::isinf(buffer.getSample(0, 256)));
    }
    std::cout << " ✅" << std::endl;

    std::cout << "  Rapid mix changes (1000 iterations)..." << std::flush;
    for (int iter = 0; iter < 1000; ++iter) {
      float value = (float)iter / 1000.0f;
      if (auto *param = apvts.getParameter("mix")) {
        param->setValueNotifyingHost(value);
      }

      processor.processBlock(buffer, midi);
      REQUIRE(!std::isnan(buffer.getSample(0, 256)));
    }
    std::cout << " ✅" << std::endl;

    return true;
  }

  static bool testExtremeAutomation() {
    std::cout << "\n🎛️  Testing Extreme Automation..." << std::endl;

    CoheraSaturatorAudioProcessor processor;
    processor.prepareToPlay(44100.0, 64); // Small block for automation

    auto &apvts = processor.getAPVTS();
    juce::AudioBuffer<float> buffer(2, 64);
    juce::MidiBuffer midi;

    std::cout << "  Per-sample parameter changes (10000 blocks)..."
              << std::flush;
    for (int block = 0; block < 10000; ++block) {
      // Change parameters every block (simulates per-sample automation)
      float phase = (float)block / 100.0f;

      if (auto *param = apvts.getParameter("drive_master")) {
        param->setValueNotifyingHost(0.5f + 0.5f * std::sin(phase));
      }
      if (auto *param = apvts.getParameter("mix")) {
        param->setValueNotifyingHost(0.5f + 0.5f * std::cos(phase));
      }

      buffer.clear();
      for (int i = 0; i < 64; ++i) {
        buffer.setSample(0, i, std::sin(2.0 * M_PI * 440.0 * i / 44100.0));
        buffer.setSample(1, i, std::sin(2.0 * M_PI * 440.0 * i / 44100.0));
      }

      processor.processBlock(buffer, midi);

      REQUIRE(!std::isnan(buffer.getSample(0, 32)));
      REQUIRE(!std::isinf(buffer.getSample(0, 32)));
    }
    std::cout << " ✅" << std::endl;

    return true;
  }

  static bool testStateStress() {
    std::cout << "\n💾 Testing State Save/Load Stress..." << std::endl;

    CoheraSaturatorAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    std::cout << "  Rapid state save/load (100 iterations)..." << std::flush;
    for (int i = 0; i < 100; ++i) {
      // Save state
      juce::MemoryBlock stateData;
      processor.getStateInformation(stateData);

      // Modify parameters
      auto &apvts = processor.getAPVTS();
      if (auto *param = apvts.getParameter("drive_master")) {
        param->setValueNotifyingHost((float)i / 100.0f);
      }

      // Restore state
      processor.setStateInformation(stateData.getData(),
                                    (int)stateData.getSize());

      // Process a block
      juce::AudioBuffer<float> buffer(2, 512);
      juce::MidiBuffer midi;
      processor.processBlock(buffer, midi);

      REQUIRE(!std::isnan(buffer.getSample(0, 256)));
    }
    std::cout << " ✅" << std::endl;

    return true;
  }

  static bool testSampleRateChanges() {
    std::cout << "\n🔄 Testing Sample Rate Changes..." << std::endl;

    CoheraSaturatorAudioProcessor processor;

    double sampleRates[] = {22050.0, 44100.0,  48000.0, 88200.0,
                            96000.0, 176400.0, 192000.0};

    std::cout << "  Testing all sample rates..." << std::flush;
    for (double sr : sampleRates) {
      processor.prepareToPlay(sr, 512);

      juce::AudioBuffer<float> buffer(2, 512);
      juce::MidiBuffer midi;

      buffer.clear();
      for (int i = 0; i < 512; ++i) {
        buffer.setSample(0, i, std::sin(2.0 * M_PI * 440.0 * i / sr));
        buffer.setSample(1, i, std::sin(2.0 * M_PI * 440.0 * i / sr));
      }

      processor.processBlock(buffer, midi);

      REQUIRE(!std::isnan(buffer.getSample(0, 256)));
      REQUIRE(!std::isinf(buffer.getSample(0, 256)));
    }
    std::cout << " ✅" << std::endl;

    return true;
  }

  static bool testPerformanceStress() {
    std::cout << "\n⏱️  Testing Performance Under Stress..." << std::endl;

    CoheraSaturatorAudioProcessor processor;
    processor.prepareToPlay(44100.0, 512);

    juce::AudioBuffer<float> buffer(2, 512);
    juce::MidiBuffer midi;

    std::cout << "  Processing 100k blocks..." << std::flush;

    auto startTime = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < 100000; ++i) {
      buffer.clear();
      for (int j = 0; j < 512; ++j) {
        buffer.setSample(0, j, std::sin(2.0 * M_PI * 440.0 * j / 44100.0));
        buffer.setSample(1, j, std::sin(2.0 * M_PI * 440.0 * j / 44100.0));
      }

      processor.processBlock(buffer, midi);
    }

    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
        endTime - startTime);

    std::cout << " ✅" << std::endl;
    std::cout << "  Time: " << duration.count() << "ms for 100k blocks"
              << std::endl;
    std::cout << "  Avg: " << (double)duration.count() / 100000.0
              << "ms per block" << std::endl;

    // Should be much faster than real-time
    // 512 samples @ 44.1kHz = ~11.6ms per block
    // We should process much faster than this
    double msPerBlock = (double)duration.count() / 100000.0;
    REQUIRE(msPerBlock < 11.6); // Must be faster than real-time

    return true;
  }
};

int main() {
  std::cout << "🧪 COHERA SATURATOR - STRESS TESTING SUITE" << std::endl;
  std::cout << "==========================================" << std::endl;

  bool allPassed = true;

  allPassed &= StressTester::testEdgeCaseParameters();
  allPassed &= StressTester::testRapidParameterChanges();
  allPassed &= StressTester::testExtremeAutomation();
  allPassed &= StressTester::testStateStress();
  allPassed &= StressTester::testSampleRateChanges();
  allPassed &= StressTester::testPerformanceStress();

  std::cout << "\n==========================================" << std::endl;
  if (allPassed) {
    std::cout << "✅ ALL STRESS TESTS PASSED!" << std::endl;
    std::cout << "\nPlugin is stable under extreme conditions:" << std::endl;
    std::cout << "  ✅ Edge case parameters handled" << std::endl;
    std::cout << "  ✅ Rapid parameter changes stable" << std::endl;
    std::cout << "  ✅ Extreme automation works" << std::endl;
    std::cout << "  ✅ State save/load robust" << std::endl;
    std::cout << "  ✅ All sample rates supported" << std::endl;
    std::cout << "  ✅ Performance exceeds real-time" << std::endl;
    return 0;
  } else {
    std::cout << "❌ SOME STRESS TESTS FAILED" << std::endl;
    return 1;
  }
}
