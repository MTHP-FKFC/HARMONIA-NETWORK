#pragma once
#include "../utils/TrackAudioFifo.h"

// Размер FFT (чем больше, тем точнее низ, но медленнее)
static constexpr int fftOrder = 11;
static constexpr int fftSize = 1 << fftOrder; // 2048
static constexpr int scopeSize = 512;         // Разрешение для рисования

class SimpleFFT {
public:
  SimpleFFT()
      : forwardFFT(fftOrder),
        window(fftSize, juce::dsp::WindowingFunction<float>::hann),
        sampleRate(44100.0f), audioFifo(1, 4096) // 1 channel, enough buffer
  {}

  void setSampleRate(float newSampleRate) { sampleRate = newSampleRate; }

  void prepare() { std::fill(scopeData.begin(), scopeData.end(), 0.0f); }

  // Вызывается из processBlock (Audio Thread) - теперь блочно и безопасно!
  void pushBlock(const juce::AudioBuffer<float> &buffer) {
    audioFifo.push(buffer);
  }

  // Deprecated single sample push (for compatibility if needed, but
  // inefficient)
  void pushSample(float sample) {
    // Not implemented efficiently, use pushBlock
  }

  // Вызывается из TimerCallback (GUI Thread)
  void process(float decay = 0.85f) {
    // Читаем данные из FIFO
    if (audioFifo.getNumReady() >= fftSize) {
      // Читаем блок данных для FFT
      juce::AudioBuffer<float> tempBuffer(1, fftSize);
      audioFifo.pull(tempBuffer);

      // Копируем в fftData
      juce::FloatVectorOperations::copy(fftData.data(),
                                        tempBuffer.getReadPointer(0), fftSize);

      // 1. Окно
      window.multiplyWithWindowingTable(fftData.data(), fftSize);

      // 2. FFT
      forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());

      // 3. Маппинг в Scope
      for (int i = 0; i < scopeSize; ++i) {
        float minFreq = 20.0f;
        float maxFreq = 20000.0f;
        float normalizedPos = (float)i / (float)(scopeSize - 1);
        float freq = minFreq * std::pow(maxFreq / minFreq, normalizedPos);
        int fftIdx = (int)((freq / (sampleRate / 2.0f)) * (fftSize / 2.0f));
        fftIdx = juce::jlimit(0, (int)(fftSize / 2) - 1, fftIdx);

        float level = fftData[(size_t)fftIdx];
        float levelDb = juce::Decibels::gainToDecibels(level) -
                        juce::Decibels::gainToDecibels((float)fftSize);
        float scaled = juce::jmap(levelDb, -100.0f, 0.0f, 0.0f, 1.0f);
        scaled = juce::jlimit(0.0f, 1.0f, scaled);

        if (scaled > scopeData[i])
          scopeData[i] = scaled;
        else
          scopeData[i] = scopeData[i] * decay + scaled * (1.0f - decay);
      }
    } else {
      // Если новых данных нет, просто затухаем старые
      for (auto &v : scopeData)
        v *= decay;
    }
  }

  const std::array<float, scopeSize> &getScopeData() const { return scopeData; }

  bool isDataReady() const { return audioFifo.getNumReady() >= fftSize; }

private:
  juce::dsp::FFT forwardFFT;
  juce::dsp::WindowingFunction<float> window;
  float sampleRate;

  Cohera::TrackAudioFifo audioFifo; // Lock-free SPSC FIFO
  std::array<float, fftSize * 2> fftData;
  std::array<float, scopeSize> scopeData;
};