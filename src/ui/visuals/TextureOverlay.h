#pragma once
#include "../../JuceHeader.h"

class TextureOverlay : public juce::Component {
public:
  TextureOverlay() {
    setInterceptsMouseClicks(false, false); // Полностью прозрачен для мыши
  }

  // Вызывать один раз после ресайза, чтобы сгенерировать картинку
  void generateTexture(int w, int h) {
    if (texture.isValid() && texture.getWidth() == w &&
        texture.getHeight() == h)
      return;

    texture = juce::Image(juce::Image::ARGB, w, h, true);
    juce::Graphics g(texture);
    juce::Random rng;

    // 1. Scanlines (черезстрочная развертка)
    g.setColour(juce::Colours::black.withAlpha(0.15f));
    for (int y = 0; y < h; y += 2) {
      g.drawHorizontalLine(y, 0.0f, (float)w);
    }

    // 2. Noise (Зерно)
    // Генерируем пиксельно для скорости
    juce::Image::BitmapData data(texture, juce::Image::BitmapData::readWrite);
    for (int y = 0; y < h; ++y) {
      for (int x = 0; x < w; ++x) {
        if (rng.nextFloat() > 0.8f) { // 20% пикселей шумные
          // Добавляем чуть-чуть белого или черного шума
          // Упрощенно: просто рисуем точки в Graphics, так как BitmapData
          // сложнее
        }
      }
    }
    // Рисуем шум через Graphics для простоты
    g.setColour(juce::Colours::white.withAlpha(0.03f));
    for (int i = 0; i < w * h / 10; ++i) {
      g.fillRect(rng.nextInt(w), rng.nextInt(h), 1, 1);
    }
  }

  // Упрощенный вариант отрисовки шума в paint (быстрее на GPU)
  void paint(juce::Graphics &g) override {
    // Рисуем сканлайны (статика)
    g.setColour(juce::Colours::black.withAlpha(0.1f));
    for (int y = 0; y < getHeight(); y += 3) {
      g.fillRect(0, y, getWidth(), 1);
    }

    // Динамический шум (каждый кадр сдвигаем фазу шума? нет, это дорого)
    // Просто зальем статичным паттерном

    // И самое главное - Цветовая коррекция (Vignette)
    // Мы уже делали это в фоне, но давай усилим углы поверх всего
    auto center = getLocalBounds().getCentre().toFloat();
    juce::ColourGradient vignette(
        juce::Colours::transparentBlack, center.x, center.y,
        juce::Colours::black.withAlpha(0.4f), 0.0f, 0.0f, true);
    g.setGradientFill(vignette);
    g.fillAll();
  }

private:
  juce::Image texture;
};
