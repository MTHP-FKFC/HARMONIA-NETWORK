#pragma once
#include "../CoheraLookAndFeel.h"
#include "AbstractVisualizer.h"

class HeadsUpDisplay : public AbstractVisualizer {
public:
  HeadsUpDisplay()
      : AbstractVisualizer(15) // 15 FPS (как старый терминал)
  {
    // Генерируем фейковую историю "CPU"
    for (int i = 0; i < 50; ++i)
      cpuHistory.push_back(random.nextFloat());
  }

protected:
  void updatePhysics() override {
    // Обновляем данные
    if (frameCount++ % 5 == 0) {
      // Сдвигаем график
      cpuHistory.erase(cpuHistory.begin());
      // Добавляем "нагрузку" (зависит от audio energy)
      float newVal =
          0.2f + (currentEnergy * 0.5f) + (random.nextFloat() * 0.1f);
      cpuHistory.push_back(newVal);

      // Обновляем Hex строки
      hexString = "0x" + juce::String::toHexString(random.nextInt())
                             .toUpperCase()
                             .substring(0, 8);
    }
  }

  void paint(juce::Graphics &g) override {
    g.setFont(juce::Font("Courier New", 10.0f, juce::Font::bold));

    // 1. CPU GRAPH (Слева внизу)
    drawMiniGraph(g, 20, getHeight() - 40, 100, 30, "CORE_LOAD");

    // 2. DATA STREAM (Справа внизу)
    g.setColour(CoheraUI::kTextDim.withAlpha(0.5f));
    g.drawText("NET_HASH: " + hexString, getWidth() - 150, getHeight() - 40,
               130, 10, juce::Justification::right);
    g.drawText("BUFFER: 512", getWidth() - 150, getHeight() - 28, 130, 10,
               juce::Justification::right);

    // 3. SYSTEM STATUS (Мигающий индикатор)
    bool blink = (juce::Time::getMillisecondCounter() / 500) % 2 == 0;
    g.setColour(blink ? CoheraUI::kAccentGreen : CoheraUI::kTextDim);
    g.fillEllipse(getWidth() - 20, getHeight() - 20, 6, 6);

    g.setColour(CoheraUI::kAccentGreen.withAlpha(0.7f));
    g.drawText("ONLINE", getWidth() - 70, getHeight() - 22, 45, 10,
               juce::Justification::right);
  }

private:
  void drawMiniGraph(juce::Graphics &g, int x, int y, int w, int h,
                     juce::String title) {
    g.setColour(CoheraUI::kTextDim.withAlpha(0.1f));
    g.drawRect(x, y, w, h);

    g.setColour(CoheraUI::kOrangeNeon.withAlpha(0.5f));
    juce::Path p;
    p.startNewSubPath((float)x, (float)(y + h));

    float stepX = (float)w / cpuHistory.size();
    for (size_t i = 0; i < cpuHistory.size(); ++i) {
      float val = cpuHistory[i];
      p.lineTo(x + i * stepX, y + h - (val * h));
    }
    g.strokePath(p, juce::PathStrokeType(1.0f));

    g.setColour(CoheraUI::kTextDim.withAlpha(0.6f));
    g.drawText(title, x, y - 12, w, 10, juce::Justification::left);
  }

  std::vector<float> cpuHistory;
  juce::String hexString = "0xDEADBEEF";
  int frameCount = 0;
  juce::Random random;
};
