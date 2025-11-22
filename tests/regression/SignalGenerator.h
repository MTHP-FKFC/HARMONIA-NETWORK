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
#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_core/juce_core.h>

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
   * Generate snare drum
   *
   * @param durationSeconds Duration (including decay)
   * @param amplitudeDB Peak amplitude
   * @param sampleRate Sample rate
   * @return Stereo audio buffer
   */
  static juce::AudioBuffer<float>
  generateSnareDrum(float durationSeconds = 1.0f, float amplitudeDB = -6.0f,
                    double sampleRate = 48000.0) {
    int numSamples = static_cast<int>(durationSeconds * sampleRate);
    juce::AudioBuffer<float> buffer(2, numSamples);

    float amplitude = juce::Decibels::decibelsToGain(amplitudeDB);

    // Snare = tone (200Hz) + noise burst
    float toneFreq = 200.0f;
    float phase = 0.0f;
    float phaseIncrement = juce::MathConstants<float>::twoPi * toneFreq /
                           static_cast<float>(sampleRate);

    juce::Random random;

    for (int i = 0; i < numSamples; ++i) {
      float t = static_cast<float>(i) / sampleRate;

      // Fast decay envelope
      float envelope = std::exp(-t * 15.0f);

      // Tone component (40%)
      float tone = std::sin(phase) * 0.4f;

      // Noise component (60%)
      float noise = (random.nextFloat() * 2.0f - 1.0f) * 0.6f;

      float sample = (tone + noise) * amplitude * envelope;
      buffer.setSample(0, i, sample);
      buffer.setSample(1, i, sample);

      phase += phaseIncrement;
      if (phase >= juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;
    }

    return buffer;
  }

  /**
   * Generate hi-hat
   *
   * @param durationSeconds Duration
   * @param amplitudeDB Peak amplitude
   * @param sampleRate Sample rate
   * @return Stereo audio buffer
   */
  static juce::AudioBuffer<float> generateHiHat(float durationSeconds = 0.5f,
                                                float amplitudeDB = -12.0f,
                                                double sampleRate = 48000.0) {
    int numSamples = static_cast<int>(durationSeconds * sampleRate);
    juce::AudioBuffer<float> buffer(2, numSamples);

    float amplitude = juce::Decibels::decibelsToGain(amplitudeDB);
    juce::Random random;

    for (int i = 0; i < numSamples; ++i) {
      float t = static_cast<float>(i) / sampleRate;

      // Very fast decay
      float envelope = std::exp(-t * 25.0f);

      // High-frequency noise (band-limited)
      float noise = (random.nextFloat() * 2.0f - 1.0f);

      // Simple high-pass (emphasize highs)
      static float lastSample = 0.0f;
      float highPassed = noise - lastSample * 0.95f;
      lastSample = noise;

      float sample = highPassed * amplitude * envelope;
      buffer.setSample(0, i, sample);
      buffer.setSample(1, i, sample);
    }

    return buffer;
  }

  /**
   * Generate bass (sine wave, low frequency)
   *
   * @param frequency Bass frequency (e.g., 55Hz for A1)
   * @param durationSeconds Duration
   * @param amplitudeDB Amplitude
   * @param sampleRate Sample rate
   * @return Stereo audio buffer
   */
  static juce::AudioBuffer<float> generateBass(float frequency = 55.0f, // A1
                                               float durationSeconds = 4.0f,
                                               float amplitudeDB = -6.0f,
                                               double sampleRate = 48000.0) {
    int numSamples = static_cast<int>(durationSeconds * sampleRate);
    juce::AudioBuffer<float> buffer(2, numSamples);

    float amplitude = juce::Decibels::decibelsToGain(amplitudeDB);
    float phase = 0.0f;
    float phaseIncrement = juce::MathConstants<float>::twoPi * frequency /
                           static_cast<float>(sampleRate);

    for (int i = 0; i < numSamples; ++i) {
      float t = static_cast<float>(i) / sampleRate;

      // Gentle ADSR envelope
      float envelope = 1.0f;
      if (t < 0.01f) {
        // Attack (10ms)
        envelope = t / 0.01f;
      } else if (t > durationSeconds - 0.1f) {
        // Release (100ms)
        envelope = (durationSeconds - t) / 0.1f;
      }

      // Pure sine for bass
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
   * Generate pink noise (guitar-like texture)
   *
   * Pink noise has equal energy per octave (more natural than white noise)
   *
   * @param durationSeconds Duration
   * @param amplitudeDB Amplitude
   * @param sampleRate Sample rate
   * @return Stereo audio buffer
   */
  static juce::AudioBuffer<float>
  generatePinkNoise(float durationSeconds = 4.0f, float amplitudeDB = -12.0f,
                    double sampleRate = 48000.0) {
    int numSamples = static_cast<int>(durationSeconds * sampleRate);
    juce::AudioBuffer<float> buffer(2, numSamples);

    float amplitude = juce::Decibels::decibelsToGain(amplitudeDB);

    // Voss-McCartney algorithm for pink noise
    for (int ch = 0; ch < 2; ++ch) {
      juce::Random random(ch + 1); // Different seed per channel

      // State variables for pink noise filter
      float b0 = 0.0f, b1 = 0.0f, b2 = 0.0f, b3 = 0.0f, b4 = 0.0f, b5 = 0.0f,
            b6 = 0.0f;

      for (int i = 0; i < numSamples; ++i) {
        float white = random.nextFloat() * 2.0f - 1.0f;

        // Paul Kellet's refined method
        b0 = 0.99886f * b0 + white * 0.0555179f;
        b1 = 0.99332f * b1 + white * 0.0750759f;
        b2 = 0.96900f * b2 + white * 0.1538520f;
        b3 = 0.86650f * b3 + white * 0.3104856f;
        b4 = 0.55000f * b4 + white * 0.5329522f;
        b5 = -0.7616f * b5 - white * 0.0168980f;

        float pink = b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362f;
        b6 = white * 0.115926f;

        float sample = pink * amplitude * 0.11f; // Normalize
        buffer.setSample(ch, i, sample);
      }
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

    auto outputStream = outputFile.createOutputStream();

    if (outputStream != nullptr) {
      juce::WavAudioFormat wavFormat;

      std::unique_ptr<juce::AudioFormatWriter> writer(wavFormat.createWriterFor(
          outputStream.release(), // Transfer ownership to writer
          sampleRate, buffer.getNumChannels(),
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
