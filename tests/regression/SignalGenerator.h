/**
 * Test Signal Generator
 *
 * Generates standard test signals for audio regression testing:
 * - Sine waves (pure tone, sweep)
 * - White noise
 * - Impulse/Click (for transient testing)
 *
 * All signals are 32-bit float, stereo, 48kHz
 */

#pragma once
#include <JuceHeader.h>
#include <cmath>

namespace Cohera {
namespace Testing {

class SignalGenerator {
public:
  /**
   * Generate sine wave at specified frequency
   *
   * @param frequency Hz (e.g., 440.0)
   * @param durationSeconds Duration in seconds
   * @param amplitudeDB Amplitude in dB (e.g., -6.0)
   * @param sampleRate Sample rate (default 48000)
   * @return Stereo audio buffer
   */
  static juce::AudioBuffer<float> generateSine(float frequency,
                                               float durationSeconds,
                                               float amplitudeDB = -6.0f,
                                               double sampleRate = 48000.0) {
    int numSamples = static_cast<int>(durationSeconds * sampleRate);
    juce::AudioBuffer<float> buffer(2, numSamples);

    float amplitude = juce::Decibels::decibelsToGain(amplitudeDB);
    float phase = 0.0f;
    float phaseIncrement = juce::MathConstants<float>::twoPi * frequency /
                           static_cast<float>(sampleRate);

    for (int i = 0; i < numSamples; ++i) {
      float sample = std::sin(phase) * amplitude;
      buffer.setSample(0, i, sample); // Left
      buffer.setSample(1, i, sample); // Right

      phase += phaseIncrement;
      if (phase >= juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;
    }

    return buffer;
  }

  /**
   * Generate sine sweep from startFreq to endFreq
   *
   * @param startFreq Starting frequency (Hz)
   * @param endFreq Ending frequency (Hz)
   * @param durationSeconds Duration
   * @param amplitudeDB Amplitude in dB
   * @param sampleRate Sample rate
   * @return Stereo audio buffer
   */
  static juce::AudioBuffer<float>
  generateSineSweep(float startFreq, float endFreq, float durationSeconds,
                    float amplitudeDB = -6.0f, double sampleRate = 48000.0) {
    int numSamples = static_cast<int>(durationSeconds * sampleRate);
    juce::AudioBuffer<float> buffer(2, numSamples);

    float amplitude = juce::Decibels::decibelsToGain(amplitudeDB);
    float phase = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
      // Linear frequency sweep
      float t = static_cast<float>(i) / numSamples;
      float currentFreq = startFreq + t * (endFreq - startFreq);

      float phaseIncrement = juce::MathConstants<float>::twoPi * currentFreq /
                             static_cast<float>(sampleRate);

      float sample = std::sin(phase) * amplitude;
      buffer.setSample(0, i, sample);
      buffer.setSample(1, i, sample);

      phase += phaseIncrement;
      if (phase >= juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;
    }

    return buffer;
  }

  /**
   * Generate white noise
   *
   * @param durationSeconds Duration
   * @param amplitudeDB Amplitude in dB
   * @param sampleRate Sample rate
   * @return Stereo audio buffer
   */
  static juce::AudioBuffer<float>
  generateWhiteNoise(float durationSeconds, float amplitudeDB = -12.0f,
                     double sampleRate = 48000.0) {
    int numSamples = static_cast<int>(durationSeconds * sampleRate);
    juce::AudioBuffer<float> buffer(2, numSamples);

    float amplitude = juce::Decibels::decibelsToGain(amplitudeDB);
    juce::Random random;

    for (int ch = 0; ch < 2; ++ch) {
      for (int i = 0; i < numSamples; ++i) {
        // Random value in [-1, 1]
        float sample = (random.nextFloat() * 2.0f - 1.0f) * amplitude;
        buffer.setSample(ch, i, sample);
      }
    }

    return buffer;
  }

  /**
   * Generate kick drum-like impulse
   *
   * @param durationSeconds Duration (including decay)
   * @param amplitudeDB Peak amplitude
   * @param sampleRate Sample rate
   * @return Stereo audio buffer
   */
  static juce::AudioBuffer<float>
  generateKickDrum(float durationSeconds = 2.0f, float amplitudeDB = -6.0f,
                   double sampleRate = 48000.0) {
    int numSamples = static_cast<int>(durationSeconds * sampleRate);
    juce::AudioBuffer<float> buffer(2, numSamples);

    float amplitude = juce::Decibels::decibelsToGain(amplitudeDB);

    // Kick parameters
    float startFreq = 150.0f; // Hz
    float endFreq = 50.0f;    // Hz
    float decayTime = 0.3f;   // seconds

    float phase = 0.0f;

    for (int i = 0; i < numSamples; ++i) {
      float t = static_cast<float>(i) / sampleRate;

      // Exponential frequency decay
      float currentFreq =
          endFreq + (startFreq - endFreq) * std::exp(-t * 10.0f);

      // Exponential amplitude decay
      float envelope = std::exp(-t / decayTime);

      float phaseIncrement = juce::MathConstants<float>::twoPi * currentFreq /
                             static_cast<float>(sampleRate);

      float sample = std::sin(phase) * amplitude * envelope;
      buffer.setSample(0, i, sample);
      buffer.setSample(1, i, sample);

      phase += phaseIncrement;
      if (phase >= juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;
    }

    return buffer;
  }

  /**
   * Save audio buffer to WAV file
   *
   * @param buffer Audio buffer to save
   * @param filepath Output file path
   * @param sampleRate Sample rate
   * @return true if successful
   */
  static bool saveToWav(const juce::AudioBuffer<float> &buffer,
                        const juce::String &filepath,
                        double sampleRate = 48000.0) {
    juce::File outputFile(filepath);
    outputFile.deleteFile(); // Remove if exists

    if (auto *outputStream = outputFile.createOutputStream()) {
      juce::WavAudioFormat wavFormat;

      std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
          outputStream, sampleRate, buffer.getNumChannels(),
          32, // 32-bit float
          {}, 0));

      if (writer != nullptr) {
        writer->writeFromAudioSampleBuffer(buffer, 0, buffer.getNumSamples());
        return true;
      }
    }

    return false;
  }

  /**
   * Load WAV file to audio buffer
   *
   * @param filepath Input file path
   * @return Audio buffer (empty if failed)
   */
  static juce::AudioBuffer<float> loadFromWav(const juce::String &filepath) {
    juce::File inputFile(filepath);

    if (!inputFile.existsAsFile()) {
      DBG("File not found: " << filepath);
      return juce::AudioBuffer<float>();
    }

    juce::AudioFormatManager formatManager;
    formatManager.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader(
        formatManager.createReaderFor(inputFile));

    if (reader != nullptr) {
      juce::AudioBuffer<float> buffer(
          static_cast<int>(reader->numChannels),
          static_cast<int>(reader->lengthInSamples));

      reader->read(&buffer, 0, static_cast<int>(reader->lengthInSamples), 0,
                   true, true);
      return buffer;
    }

    return juce::AudioBuffer<float>();
  }
};

} // namespace Testing
} // namespace Cohera
