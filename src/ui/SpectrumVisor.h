#pragma once

#include "CoheraLookAndFeel.h"
#include <juce_gui_basics/juce_gui_basics.h>

class SpectrumVisor : public juce::Component, private juce::Timer {
public:
  SpectrumVisor() {
    // Инициализируем предыдущие данные
    prevFftData.fill(0.0f);

    // Инициализируем HUD
    for (int i = 0; i < 50; ++i)
      cpuHistory.push_back(0.0f);

    // Timer starts in visibilityChanged()
  }

  ~SpectrumVisor() override { stopTimer(); }
  
  // Smart timer management (stop when hidden)
  void visibilityChanged() override {
    if (isVisible())
      startTimerHz(30);
    else
      stopTimer();
  }

  // Установка реальных FFT данных
  void setFFTData(const std::array<float, 512> &data) {
    // Сглаживаем данные для предотвращения резких прыжков
    const float smoothingFactor = 0.7f; // Чем выше, тем плавнее

    for (size_t i = 0; i < data.size(); ++i) {
      // Экспоненциальное сглаживание
      fftData[i] =
          prevFftData[i] * smoothingFactor + data[i] * (1.0f - smoothingFactor);

      // Ограничиваем диапазон
      fftData[i] = juce::jlimit(0.0f, 1.0f, fftData[i]);

      // Сохраняем для следующего кадра
      prevFftData[i] = fftData[i];
    }

    repaint();
  }

  void resized() override {
    auto h = (float)getHeight();
    
    // Cache gradient once per resize (performance optimization)
    cachedGradient = juce::ColourGradient(
        juce::Colour::fromRGB(255, 140, 0).withAlpha(0.6f), 0, 0,
        juce::Colour::fromRGB(255, 140, 0).withAlpha(0.0f), 0, h,
        false
    );
  }
  
  void timerCallback() override {
    // Анимация "плавания" HUD (как в телевизоре)
    float time = juce::Time::getMillisecondCounter() / 1000.0f;
    hudOffsetY = std::sin(time * 0.5f) * 5.0f;
    hudOffsetX = std::cos(time * 0.3f) * 3.0f;

    // Обновляем фейковые данные CPU
    cpuHistory.erase(cpuHistory.begin());
    float cpuLoad = 0.2f + (juce::Random::getSystemRandom().nextFloat() * 0.3f);
    // Если есть сигнал, нагрузка выше
    if (fftData[10] > 0.1f)
      cpuLoad += fftData[10] * 0.5f;
    cpuHistory.push_back(juce::jlimit(0.0f, 1.0f, cpuLoad));

    // Обновляем Hex иногда
    if ((int)(time * 30) % 60 == 0) {
      hexString = "0x" + juce::String::toHexString(
                             juce::Random::getSystemRandom().nextInt())
                             .toUpperCase()
                             .substring(0, 8);
    }

    repaint();
  }

  void paint(juce::Graphics &g) override {
    auto bounds = getLocalBounds().toFloat();
    auto area = bounds.reduced(10.0f);

    // 1. Фон (Темное стекло)
    g.setColour(CoheraUI::kPanel.darker(0.3f));
    g.fillRoundedRectangle(bounds, 6.0f);

    // 1.5. Sacred Geometry (Золотая Спираль)
    drawSacredGeometry(g, area.getWidth(), area.getHeight());

    // 2. Сетка (Grid)
    drawGrid(g, area.getWidth(), area.getHeight());

    // 3. Спектр (Реальные FFT данные)
    if (!fftData.empty()) {
      // Конвертируем array в vector для совместимости
      std::vector<float> fftVector(fftData.begin(), fftData.end());
      drawSpectrum(g, area.getWidth(), area.getHeight(), fftVector,
                   CoheraUI::kOrangeNeon, true);
    } else {
      // Fallback на dummy data если FFT не готов
      std::vector<float> dummyData = {0.1f, 0.3f, 0.8f, 0.6f, 0.2f,
                                      0.4f, 0.9f, 0.5f, 0.1f};
      drawSpectrum(g, area.getWidth(), area.getHeight(), dummyData,
                   CoheraUI::kOrangeNeon, true);
    }

    // 3.5 HUD (Информационные приколюхи)
    drawHUD(g, area);

    // 4. Стекло (Glass Reflection)
    juce::ColourGradient glare(juce::Colours::white.withAlpha(0.05f), 0, 0,
                               juce::Colours::transparentWhite, 0,
                               area.getHeight() * 0.4f, false);
    g.setGradientFill(glare);
    g.fillRoundedRectangle(area.removeFromTop(area.getHeight() * 0.4f), 4.0f);

    // Рамка
    g.setColour(CoheraUI::kTextDim.withAlpha(0.2f));
    g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
  }

private:
  // Вспомогательная функция для рисования спектра с градиентной заливкой
  void drawSpectrum(juce::Graphics &g, float w, float h,
                    const std::vector<float> &data, juce::Colour color,
                    bool fill) {
    if (data.empty())
      return;

    juce::Path fillPath;
    juce::Path strokePath;

    float step = w / (data.size() - 1);

    // Строим Path
    fillPath.startNewSubPath(0, h); // Для заливки начинаем снизу

    bool firstPoint = true;

    // Рисуем спектр
    for (size_t i = 0; i < data.size(); ++i) {
      float x = i * step;

      // ФИКСИРУЕМ ЛЕВЫЙ КРАЙ - первые 3 бины всегда на минимальном уровне
      float spectrumValue;
      if (i < 3) {
        spectrumValue =
            0.0f; // Минимальный уровень для низких частот (левый край)
      } else {
        spectrumValue = data[i];
      }

      // Стабилизируем Y - ограничиваем диапазон и сглаживаем
      float rawY = spectrumValue * h * 0.8f;
      float clampedY =
          juce::jlimit(0.0f, h * 0.8f, rawY); // Не даем уходить за пределы
      float y = h - clampedY;

      fillPath.lineTo(x, y);

      if (firstPoint) {
        strokePath.startNewSubPath(x, y);
        firstPoint = false;
      } else {
        strokePath.lineTo(x, y);
      }
    }

    // Завершаем путь заливки
    fillPath.lineTo(w, h);   // Вниз вправо
    fillPath.closeSubPath(); // Замыкаем влево

    if (fill) {
      // 🔥 ГРАДИЕНТНАЯ ЗАЛИВКА (using cached gradient)
      g.setGradientFill(cachedGradient);
      g.fillPath(fillPath);

      // Обводка (Sharp Line) - рисуем ТОЛЬКО верхнюю линию, без замыкания
      g.setColour(color);
      g.strokePath(strokePath, juce::PathStrokeType(1.5f));
    } else {
      g.setColour(color);
      g.strokePath(strokePath, juce::PathStrokeType(2.0f));
    }
  }

  void drawHUD(juce::Graphics &g, juce::Rectangle<float> area) {
    // "Phone Settings" style list
    // Располагаем справа сверху, плавающим блоком

    float w = 140.0f;
    float h = 100.0f;
    float x = area.getRight() - w - 10.0f + hudOffsetX;
    float y = area.getY() + 10.0f + hudOffsetY;

    auto hudBounds = juce::Rectangle<float>(x, y, w, h);

    // Полупрозрачный фон списка
    g.setColour(juce::Colours::black.withAlpha(0.4f));
    g.fillRoundedRectangle(hudBounds, 4.0f);
    g.setColour(CoheraUI::kTextDim.withAlpha(0.2f));
    g.drawRoundedRectangle(hudBounds, 4.0f, 1.0f);

    // Рисуем элементы списка
    float rowH = 20.0f;
    float currY = y + 5.0f;
    float padX = 10.0f;

    g.setFont(10.0f);

    // 1. Network Status
    drawHUDItem(g, x, currY, w, rowH, "STATUS", "ONLINE",
                CoheraUI::kAccentGreen);
    currY += rowH;

    // 2. Buffer
    drawHUDItem(g, x, currY, w, rowH, "BUFFER", "512", CoheraUI::kTextDim);
    currY += rowH;

    // 3. Net Hash
    drawHUDItem(g, x, currY, w, rowH, "NET HASH", hexString,
                CoheraUI::kTextDim);
    currY += rowH;

    // 4. Core Load (Graph)
    g.setColour(CoheraUI::kTextDim.withAlpha(0.6f));
    g.drawText("CORE LOAD", x + padX, currY, 60, rowH,
               juce::Justification::centredLeft);

    // Mini Graph
    float graphX = x + 70.0f;
    float graphW = w - 80.0f;
    float graphH = 14.0f;
    drawMiniGraph(g, graphX, currY + 3, graphW, graphH);
  }

  void drawHUDItem(juce::Graphics &g, float x, float y, float w, float h,
                   juce::String label, juce::String value,
                   juce::Colour valColor) {
    float padX = 10.0f;
    g.setColour(CoheraUI::kTextDim.withAlpha(0.6f));
    g.drawText(label, x + padX, y, w * 0.5f, h,
               juce::Justification::centredLeft);

    g.setColour(valColor);
    g.drawText(value, x + w * 0.5f, y, w * 0.5f - padX, h,
               juce::Justification::centredRight);

    // Separator line
    g.setColour(CoheraUI::kTextDim.withAlpha(0.1f));
    g.drawHorizontalLine((int)(y + h), x + 5.0f, x + w - 5.0f);
  }

  void drawMiniGraph(juce::Graphics &g, float x, float y, float w, float h) {
    g.setColour(CoheraUI::kTextDim.withAlpha(0.1f));
    g.fillRect(x, y, w, h);

    g.setColour(CoheraUI::kOrangeNeon.withAlpha(0.7f));
    juce::Path p;

    float stepX = w / (float)cpuHistory.size();
    bool first = true;

    for (size_t i = 0; i < cpuHistory.size(); ++i) {
      float val = cpuHistory[i];
      float px = x + i * stepX;
      float py = y + h - (val * h);

      if (first) {
        p.startNewSubPath(px, py);
        first = false;
      } else {
        p.lineTo(px, py);
      }
    }
    g.strokePath(p, juce::PathStrokeType(1.0f));
  }

  // Рисуем сетку в стиле радара
  void drawGrid(juce::Graphics &g, float w, float h) {
    // Тонкие линии
    g.setColour(CoheraUI::kTextDim.withAlpha(0.08f));

    // Вертикальные (частоты)
    float freqs[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000};
    for (auto f : freqs) {
      float x = mapFreqToX(f, w);
      // Dash line
      float dash[] = {2.0f, 2.0f};
      juce::Line<float> line(x, 0, x, h);
      g.drawDashedLine(line, dash, 2);

      // Labels (только для основных)
      if (f == 100 || f == 1000 || f == 10000) {
        juce::String t =
            (f >= 1000) ? juce::String(f / 1000) + "k" : juce::String((int)f);
        g.setFont(10.0f);
        g.setColour(CoheraUI::kTextDim.withAlpha(0.4f));
        g.drawText(t, x + 3, h - 14, 30, 12, juce::Justification::left);
      }
    }

    // Горизонтальные (dB)
    float dbLevels[] = {-60, -40, -20, 0};
    for (auto db : dbLevels) {
      float y = mapDbToY(db, h);
      float dash[] = {2.0f, 2.0f};
      juce::Line<float> line(0, y, w, y);
      g.drawDashedLine(line, dash, 2);

      // Labels
      g.setFont(9.0f);
      g.setColour(CoheraUI::kTextDim.withAlpha(0.4f));
      g.drawText(juce::String(db), 5, y - 5, 25, 10, juce::Justification::left);
    }
  }

  // Sacred Geometry (Золотая Спираль Фибоначчи)
  void drawSacredGeometry(juce::Graphics &g, float w, float h) {
    g.setColour(CoheraUI::kTextDim.withAlpha(0.03f)); // Едва заметно

    // Рисуем Золотую Спираль (приблизительно)
    juce::Path spiral;
    float x = w * 0.1f, y = h * 0.8f;
    float boxW = w * 0.8f;
    float boxH = h * 0.6f;

    spiral.startNewSubPath(x, y + boxH);

    // Несколько итераций золотого сечения
    for (int i = 0; i < 6; ++i) {
      // Это упрощенная визуализация, главное - эстетика
      spiral.cubicTo(x, y, x + boxW, y, x + boxW, y + boxH);

      // Уменьшаем бокс по Phi (золотое сечение)
      float newW = boxW / 1.618f;
      float newH = boxH / 1.618f;
      x += (boxW - newW) / 2;
      y += (boxH - newH) / 2;
      boxW = newW;
      boxH = newH;
    }

    g.strokePath(spiral, juce::PathStrokeType(1.0f));

    // Добавим пару окружностей "Сакральной геометрии"
    float centerX = w * 0.5f;
    float centerY = h * 0.5f;
    float radius1 = 80.0f;
    float radius2 = radius1 / 1.618f; // Золотое сечение

    g.drawEllipse(centerX - radius1, centerY - radius1, radius1 * 2,
                  radius1 * 2, 1.0f);
    g.drawEllipse(centerX - radius2, centerY - radius2, radius2 * 2,
                  radius2 * 2, 1.0f);
  }

  // Вспомогательные функции для маппинга
  float mapFreqToX(float freq, float width) {
    // Логарифмическое маппинг частот (20Hz - 20kHz)
    float minFreq = 20.0f;
    float maxFreq = 20000.0f;
    float logFreq = std::log10(freq / minFreq) / std::log10(maxFreq / minFreq);
    return logFreq * width;
  }

  float mapDbToY(float db, float height) {
    // dB маппинг (-60dB = bottom, 0dB = top)
    float minDb = -60.0f;
    float maxDb = 0.0f;
    return height - ((db - minDb) / (maxDb - minDb)) * height;
  }

private:
  // Реальные FFT данные от процессора
  std::array<float, 512> fftData;
  // Предыдущие данные для сглаживания
  std::array<float, 512> prevFftData;

  // HUD Data
  std::vector<float> cpuHistory;
  juce::String hexString = "0xDEADBEEF";
  float hudOffsetX = 0.0f;
  float hudOffsetY = 0.0f;
  
  // Cached resources (performance optimization - create once, use in paint)
  juce::ColourGradient cachedGradient;
};
