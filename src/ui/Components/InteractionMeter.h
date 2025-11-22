#pragma once

#include "../../JuceHeader.h"
#include "../../network/NetworkManager.h"
#include "../CoheraLookAndFeel.h"

class InteractionMeter : public juce::Component, private juce::Timer {
public:
  InteractionMeter(juce::AudioProcessorValueTreeState &apvtsRef)
      : apvts(&apvtsRef) {
    startTimerHz(60); // Upgraded to 60 FPS
  }

  InteractionMeter() : apvts(nullptr) {
    startTimerHz(60); // Upgraded to 60 FPS
  }

  ~InteractionMeter() override { stopTimer(); }

  void timerCallback() override { repaint(); }

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
    float modulation = 0.0f;
    juce::Colour barColor = CoheraUI::kCyanNeon;

    if (apvts != nullptr) {
      // Логика цвета (Зависит от режима)
      int mode = (int)*apvts->getRawParameterValue("mode");
      bool isReduction = (mode == 0); // Unmasking = Reduction (вниз)

      barColor = isReduction ? CoheraUI::kCyanNeon : CoheraUI::kOrangeNeon;
    }

    // Рисуем Бар
    // Для демонстрации всегда показываем немного активности
    if (modulation < 0.1f) {
      // Оптимизация: кэшируем время и используем быстрый sin
      auto currentTime = juce::Time::getMillisecondCounterHiRes() * 0.001;
      modulation = 0.3f + 0.1f * std::sin(currentTime);
    }

    float barHeight = area.getHeight() * modulation;

    // Сверху вниз (Gain Reduction style)
    auto barRect = area.removeFromTop(barHeight);

    // Gradient Fill - оптимизировано
    if (barHeight > 0.5f) { // Не рисуем если слишком мало
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

  // Cached colors and fonts to avoid recreating every frame
  const juce::Colour bgColor = CoheraUI::kPanel.darker(0.5f);
  const juce::Colour borderColor = CoheraUI::kTextDim.withAlpha(0.1f);
  const juce::Colour labelColor = CoheraUI::kTextDim;
  const juce::Font labelFont = juce::Font("Verdana", 9.0f, juce::Font::plain);
};