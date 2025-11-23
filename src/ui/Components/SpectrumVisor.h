#pragma once
#include "../../JuceHeader.h"
#include "../../network/NetworkManager.h"
#include "../../ui/SimpleFFT.h"

class SpectrumVisor : public juce::Component, private juce::Timer {
public:
  SpectrumVisor(SimpleFFT &fftToUse, juce::AudioProcessorValueTreeState &apvts)
      : fft(fftToUse), apvts(apvts) {
    // Не стартуем таймер здесь - только когда visible
  }

  ~SpectrumVisor() override {
    stopTimer();

#ifndef NDEBUG
    if (frameCount > 0) {
      DBG("SpectrumVisor Performance Stats:");
      DBG("  Frames: " << frameCount);
      DBG("  Avg: " << juce::String(avgPaintTime, 3) << " ms");
      DBG("  Min: " << juce::String(minPaintTime, 3) << " ms");
      DBG("  Max: " << juce::String(maxPaintTime, 3) << " ms");
      DBG("  P95: " << juce::String(p95PaintTime, 3) << " ms");
      DBG("  Frame drops: " << frameDrops);
    }
#endif
  }

  void visibilityChanged() override { updateTimerState(); }

  void parentHierarchyChanged() override { updateTimerState(); }

  void timerCallback() override {
    fft.process(0.85f);
    repaint();
  }

  void resized() override {
    Component::resized();
    updateGradient();
  }

  void paint(juce::Graphics &g) override {
#ifndef NDEBUG
    auto startTime = juce::Time::getMillisecondCounterHiRes();
#endif

    auto area = getLocalBounds().toFloat();
    auto w = area.getWidth();
    auto h = area.getHeight();

    // 1. Фон
    g.setColour(juce::Colour::fromRGB(15, 17, 20));
    g.fillRoundedRectangle(area, 4.0f);

    // 2. Сетка (Grid)
    g.setColour(juce::Colours::white.withAlpha(0.05f));
    float freqPoints[] = {100.0f, 1000.0f, 10000.0f};
    for (float freq : freqPoints) {
      float x = mapFreqToX(freq, w);
      g.drawVerticalLine((int)x, 0.0f, h);

      g.setFont(10.0f);
      g.drawText(juce::String((int)freq) + "Hz", (int)x + 2, (int)h - 12, 40,
                 10, juce::Justification::left);
    }

    // 3. Спектр
    const auto &data = fft.getScopeData();

    spectrumPath.clear();
    spectrumPath.startNewSubPath(0, h);

    for (int i = 0; i < (int)data.size(); ++i) {
      float x = juce::jmap((float)i, 0.0f, (float)data.size(), 0.0f, w);
      float y = juce::jmap(data[i], 0.0f, 1.0f, h, 0.0f);
      spectrumPath.lineTo(x, y);
    }

    spectrumPath.lineTo(w, h);
    spectrumPath.closeSubPath();

    // Используем cached gradient
    g.setGradientFill(cachedSpectrumGradient);
    g.fillPath(spectrumPath);

    // Рисуем контур
    g.setColour(juce::Colour::fromRGB(255, 200, 100));
    g.strokePath(spectrumPath, juce::PathStrokeType(1.5f));

    // 4. Ghost Spectrum
    bool isRef = *apvts.getRawParameterValue("role") > 0.5f;
    if (!isRef) {
      drawGhostCurve(g, w, h);
    }

#ifndef NDEBUG
    auto endTime = juce::Time::getMillisecondCounterHiRes();
    auto paintTime = endTime - startTime;

    frameCount++;

    // Running statistics
    if (frameCount == 1) {
      minPaintTime = maxPaintTime = avgPaintTime = paintTime;
    } else {
      minPaintTime = juce::jmin(minPaintTime, paintTime);
      maxPaintTime = juce::jmax(maxPaintTime, paintTime);
      avgPaintTime = (avgPaintTime * (frameCount - 1) + paintTime) / frameCount;
    }

    // Frame drop detection
    if (paintTime > 16.67) {
      frameDrops++;
    }

    // P95 calculation (simplified - store last 100 samples)
    paintTimeSamples[sampleIndex % 100] = paintTime;
    sampleIndex++;

    if (frameCount % 100 == 0) {
      std::vector<double> sorted(paintTimeSamples.begin(),
                                 paintTimeSamples.end());
      std::sort(sorted.begin(), sorted.end());
      p95PaintTime = sorted[95];
    }
#endif
  }

  float mapFreqToX(float freq, float w) {
    return w * (std::log10(freq / 20.0f) / std::log10(20000.0f / 20.0f));
  }

  void drawGhostCurve(juce::Graphics &g, float w, float h) {
    int group = (int)*apvts.getRawParameterValue("group_id");
    auto &net = NetworkManager::getInstance();

    ghostPath.clear();
    bool first = true;

    float bandXPositions[6] = {0.15f, 0.3f, 0.45f, 0.6f, 0.75f, 0.9f};

    for (int i = 0; i < 6; ++i) {
      float energy = net.getBandSignal(group, i);
      float x = w * bandXPositions[i];
      float y = h - (energy * h * 0.9f);

      if (first) {
        ghostPath.startNewSubPath(0, h);
        ghostPath.lineTo(x, y);
        first = false;
      } else {
        ghostPath.lineTo(x, y);
      }
    }
    ghostPath.lineTo(w, h);

    g.setColour(juce::Colour::fromRGB(0, 255, 200).withAlpha(0.4f));
    g.strokePath(ghostPath,
                 juce::PathStrokeType(2.0f, juce::PathStrokeType::curved));

    g.setColour(juce::Colour::fromRGB(0, 255, 200).withAlpha(0.1f));
    ghostPath.closeSubPath();
    g.fillPath(ghostPath);
  }

private:
  SimpleFFT &fft;
  juce::AudioProcessorValueTreeState &apvts;

  // Reusable paths
  juce::Path spectrumPath;
  juce::Path ghostPath;

  // Cached gradient (updated on resize)
  juce::ColourGradient cachedSpectrumGradient;

// Enhanced performance metrics
#ifndef NDEBUG
  int frameCount = 0;
  int frameDrops = 0;
  int sampleIndex = 0;
  double minPaintTime = 0.0;
  double avgPaintTime = 0.0;
  double maxPaintTime = 0.0;
  double p95PaintTime = 0.0;
  std::array<double, 100> paintTimeSamples{};
#endif

  // Smart timer management
  void updateTimerState() {
    if (isVisible() && isShowing()) {
      if (!isTimerRunning()) {
        startTimerHz(60);
      }
    } else {
      if (isTimerRunning()) {
        stopTimer();
      }
    }
  }

  // Update gradient on resize
  void updateGradient() {
    auto h = getHeight();
    cachedSpectrumGradient = juce::ColourGradient(
        juce::Colour::fromRGB(255, 140, 0).withAlpha(0.6f), 0, 0,
        juce::Colour::fromRGB(255, 140, 0).withAlpha(0.0f), 0, h, false);
  }
};