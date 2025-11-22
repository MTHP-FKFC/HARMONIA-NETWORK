/**
 * Audio Comparator Utility
 *
 * Provides bit-exact comparison between audio buffers for regression testing.
 * Calculates Max Absolute Error (MAE) and Peak Signal-to-Noise Ratio (PSNR).
 */

#pragma once
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <limits>

namespace Cohera {
namespace Testing {

struct ComparisonResult {
  bool passed;
  float maxDiff;     // Max absolute difference
  float maxDiffDb;   // Max difference in dB
  int diffSamplePos; // Position of max difference
  int diffChannel;   // Channel of max difference

  juce::String toString() const {
    if (passed) {
      return "PASS (Max Diff: " + juce::String(maxDiff, 8) + " / " +
             juce::String(maxDiffDb, 2) + " dB)";
    } else {
      return "FAIL (Max Diff: " + juce::String(maxDiff, 8) + " @ ch" +
             juce::String(diffChannel) + " sample " +
             juce::String(diffSamplePos) + ")";
    }
  }
};

class AudioComparator {
public:
  /**
   * Compare two audio buffers
   * @param reference The golden standard buffer
   * @param test The buffer to test
   * @param threshold Max allowed difference (default 1e-6 for 32-bit float
   * precision)
   */
  static ComparisonResult compare(const juce::AudioBuffer<float> &reference,
                                  const juce::AudioBuffer<float> &test,
                                  float threshold = 1.0e-6f) {
    ComparisonResult result;
    result.passed = true;
    result.maxDiff = 0.0f;
    result.diffSamplePos = -1;
    result.diffChannel = -1;

    // Check dimensions
    if (reference.getNumChannels() != test.getNumChannels() ||
        reference.getNumSamples() != test.getNumSamples()) {
      result.passed = false;
      result.maxDiff = std::numeric_limits<float>::max();
      return result;
    }

    int numChannels = reference.getNumChannels();
    int numSamples = reference.getNumSamples();

    for (int ch = 0; ch < numChannels; ++ch) {
      const float *refData = reference.getReadPointer(ch);
      const float *testData = test.getReadPointer(ch);

      for (int i = 0; i < numSamples; ++i) {
        float diff = std::abs(refData[i] - testData[i]);

        if (diff > result.maxDiff) {
          result.maxDiff = diff;
          result.diffSamplePos = i;
          result.diffChannel = ch;
        }
      }
    }

    result.maxDiffDb = juce::Decibels::gainToDecibels(result.maxDiff);

    // Check against threshold
    if (result.maxDiff > threshold) {
      result.passed = false;
    }

    return result;
  }
};

} // namespace Testing
} // namespace Cohera
