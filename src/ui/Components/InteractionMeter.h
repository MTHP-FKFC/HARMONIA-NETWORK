#pragma once

#include "../../JuceHeader.h"
#include "../../network/NetworkManager.h"
#include "../CoheraLookAndFeel.h"

class InteractionMeter : public juce::Component, private juce::Timer {
public:
  InteractionMeter(juce::AudioProcessorValueTreeState &apvtsRef)
      : apvts(&apvtsRef) {
    // Не стартуем таймер - только когда visible
  }

  InteractionMeter() : apvts(nullptr) {
    // Не стартуем таймер
  }

  ~InteractionMeter() override { stopTimer(); }

  void visibilityChanged() override { updateTimerState(); }

  void parentHierarchyChanged() override { updateTimerState(); }

  void timerCallback() override {
    updateModulation();
    repaint();
  }

  void setAPVTS(juce::AudioProcessorValueTreeState &newApvts) {
    apvts = &newApvts;
  }

  void paint(juce::Graphics &g) override {
    auto area = getLocalBounds().toFloat().reduced(4.0f);

    // Фон (Slot) - кэшируем цвета
    g.setColour(bgColor);
    g.fillRoundedRectangle(area, 4.0f);
    g.setColour(borderColor);
    g.drawRoundedRectangle(area, 4.0f, 1.0f);

    // Данные
    juce::Colour barColor = CoheraUI::kCyanNeon;

    if (apvts != nullptr) {
      int mode = (int)*apvts->getRawParameterValue("mode");
      bool isReduction = (mode == 0);
      barColor = isReduction ? CoheraUI::kCyanNeon : CoheraUI::kOrangeNeon;
    }

    // Используем cached modulation
    float barHeight = area.getHeight() * cachedModulation;

    // Рисуем бар только если достаточно большой
    if (barHeight > 0.5f) {
      auto barRect = area.removeFromTop(barHeight);

      juce::ColourGradient grad(barColor, barRect.getCentreX(), barRect.getY(),
                                barColor.darker(0.5f), barRect.getCentreX(),
                                barRect.getBottom(), false);

      g.setGradientFill(grad);
      g.fillRoundedRectangle(barRect, 4.0f);
    }

    // Текстовая метка - кэшируем
    g.setColour(labelColor);
    g.setFont(labelFont);
    g.drawText("ACTIVITY", getLocalBounds().removeFromBottom(12),
               juce::Justification::centred);
  }

private:
  juce::AudioProcessorValueTreeState *apvts;

  // Cached colors and fonts
  const juce::Colour bgColor = CoheraUI::kPanel.darker(0.5f);
  const juce::Colour borderColor = CoheraUI::kTextDim.withAlpha(0.1f);
  const juce::Colour labelColor = CoheraUI::kTextDim;
  const juce::Font labelFont{"Verdana", 9.0f, juce::Font::plain};

  // Cached animation state
  float cachedModulation = 0.3f;
  double lastUpdateTime = 0.0;

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

  // Optimize sin calculation (cache result)
  void updateModulation() {
    auto currentTime = juce::Time::getMillisecondCounterHiRes() * 0.001;

    if (currentTime - lastUpdateTime > 0.016) { // ~60Hz
      cachedModulation = 0.3f + 0.1f * std::sin(currentTime);
      lastUpdateTime = currentTime;
    }
  }
};