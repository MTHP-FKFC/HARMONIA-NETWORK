#pragma once
#include "../CoheraLookAndFeel.h"
#include "AbstractVisualizer.h"

class HorizonGrid : public AbstractVisualizer {
public:
  HorizonGrid() : AbstractVisualizer(30) {} // 30 FPS хватит для фона

protected:
  void updatePhysics() override {
    // Медленный полет вперед + ускорение от энергии
    offset += 0.002f + (currentEnergy * 0.05f);
    if (offset > 1.0f)
      offset -= 1.0f;

    // Пульсация яркости от баса
    glow = 0.1f + (currentEnergy * 0.4f);
  }

  void paint(juce::Graphics &g) override {
    float w = (float)getWidth();
    float h = (float)getHeight();
    float horizonY =
        h * 0.4f; // Горизонт чуть выше середины (скрыт за панелями)

    // Градиент исчезновения к горизонту
    juce::Colour baseCol = CoheraUI::kCyanNeon; // Цвет сетки

    // 1. ВЕРТИКАЛЬНЫЕ ЛИНИИ (Перспектива)
    // Расходятся из центра горизонта
    int numVLines = 20;
    g.setColour(baseCol.withAlpha(glow * 0.3f));

    for (int i = -numVLines; i <= numVLines; ++i) {
      float xProp = (float)i / (float)numVLines; // -1..1
      // Простая формула перспективы: чем ниже, тем шире
      float xTop = w * 0.5f + (xProp * w * 0.1f); // Узко на горизонте
      float xBot = w * 0.5f + (xProp * w * 2.0f); // Широко внизу

      // Рисуем только нижнюю часть
      juce::Line<float> line(xTop, horizonY, xBot, h);

      // Альфа-маска (исчезает к горизонту)
      juce::ColourGradient fade(juce::Colours::transparentBlack, xTop, horizonY,
                                baseCol.withAlpha(glow * 0.5f), xBot, h, false);
      g.setGradientFill(fade);
      // Рисуем линию как трапецию для градиента или просто линию с
      // прозрачностью Проще:
      g.drawLine(line, 1.0f);
    }

    // 2. ГОРИЗОНТАЛЬНЫЕ ЛИНИИ (Бегущие)
    // Расстояние между ними логарифмическое
    int numHLines = 15;
    for (int i = 0; i < numHLines; ++i) {
      // t = 0 (горизонт) .. 1 (низ экрана)
      float rawT = (float)i / numHLines + offset;
      if (rawT > 1.0f)
        rawT -= 1.0f;

      // Перспективное искажение: y = t^3
      float t = rawT * rawT * rawT;

      float y = horizonY + t * (h - horizonY);

      // Яркость зависит от близости к камере
      float alpha = t * glow;
      g.setColour(baseCol.withAlpha(alpha));

      g.drawHorizontalLine((int)y, 0.0f, w);
    }
  }

private:
  float offset = 0.0f;
  float glow = 0.0f;
};
