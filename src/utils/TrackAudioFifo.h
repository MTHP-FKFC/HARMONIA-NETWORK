/*
  ==============================================================================

    TrackAudioFifo.h
    Created: 22 Nov 2025
    Author:  Cohera

    Thread-safe SPSC (Single Producer Single Consumer) FIFO for audio data.
    Designed for safe transfer from Audio Thread to Analysis/UI Thread.

  ==============================================================================
*/

#pragma once
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <vector>

namespace Cohera {

class TrackAudioFifo {
public:
  TrackAudioFifo(int numChannels, int numSamples) {
    // Ensure buffer size is power of 2 for efficiency
    fifo.setTotalSize(numSamples);

    buffer.setSize(numChannels, numSamples);
  }

  void push(const juce::AudioBuffer<float> &data) {
    int numSamples = data.getNumSamples();
    int numChannels = data.getNumChannels();

    // Check if we have enough space
    if (fifo.getFreeSpace() < numSamples)
      return; // Drop data if full (better than blocking)

    int start1, size1, start2, size2;
    fifo.prepareToWrite(numSamples, start1, size1, start2, size2);

    for (int ch = 0; ch < numChannels; ++ch) {
      if (ch >= buffer.getNumChannels())
        break;

      if (size1 > 0)
        buffer.copyFrom(ch, start1, data, ch, 0, size1);
      if (size2 > 0)
        buffer.copyFrom(ch, start2, data, ch, size1, size2);
    }

    fifo.finishedWrite(size1 + size2);
  }

  void pull(juce::AudioBuffer<float> &destination) {
    int numSamples = destination.getNumSamples();
    int numChannels = destination.getNumChannels();
    int available = fifo.getNumReady();

    if (available < numSamples) {
      destination.clear();
      return;
    }

    int start1, size1, start2, size2;
    fifo.prepareToRead(numSamples, start1, size1, start2, size2);

    for (int ch = 0; ch < numChannels; ++ch) {
      if (ch >= buffer.getNumChannels())
        break;

      if (size1 > 0)
        destination.copyFrom(ch, 0, buffer, ch, start1, size1);
      if (size2 > 0)
        destination.copyFrom(ch, size1, buffer, ch, start2, size2);
    }

    fifo.finishedRead(size1 + size2);
  }

  // Pull all available samples into a std::vector (useful for visualization)
  void pullToVector(std::vector<float> &destination, int channel) {
    int available = fifo.getNumReady();
    if (available == 0)
      return;

    int start1, size1, start2, size2;
    fifo.prepareToRead(available, start1, size1, start2, size2);

    if (channel < buffer.getNumChannels()) {
      // Append to vector
      const float *readPtr = buffer.getReadPointer(channel);

      if (size1 > 0)
        destination.insert(destination.end(), readPtr + start1,
                           readPtr + start1 + size1);
      if (size2 > 0)
        destination.insert(destination.end(), readPtr + start2,
                           readPtr + start2 + size2);
    }

    fifo.finishedRead(size1 + size2);
  }

  int getNumReady() const { return fifo.getNumReady(); }
  int getFreeSpace() const { return fifo.getFreeSpace(); }

private:
  juce::AbstractFifo fifo{1024};
  juce::AudioBuffer<float> buffer;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(TrackAudioFifo)
};

} // namespace Cohera
