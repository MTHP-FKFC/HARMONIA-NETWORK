#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CoheraLookAndFeel.h"

class SpectrumVisor : public juce::Component
{
public:
    SpectrumVisor() {}

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto area = bounds.reduced(10.0f);

        // 1. Фон (Темное стекло)
        g.setColour(CoheraUI::kPanel.darker(0.3f));
        g.fillRoundedRectangle(bounds, 6.0f);

        // 1.5. Sacred Geometry (Золотая Спираль)
        drawSacredGeometry(g, area.getWidth(), area.getHeight());

        // 2. Сетка (Grid)
        drawGrid(g, area.getWidth(), area.getHeight());

        // 3. Спектр (Placeholder - градиентная заливка)
        std::vector<float> dummyData = {0.1f, 0.3f, 0.8f, 0.6f, 0.2f, 0.4f, 0.9f, 0.5f, 0.1f};
        drawSpectrum(g, area.getWidth(), area.getHeight(), dummyData, CoheraUI::kOrangeNeon, true);

        // 4. Стекло (Glass Reflection)
        juce::ColourGradient glare(
            juce::Colours::white.withAlpha(0.05f), 0, 0,
            juce::Colours::transparentWhite, 0, area.getHeight() * 0.4f, false);
        g.setGradientFill(glare);
        g.fillRoundedRectangle(area.removeFromTop(area.getHeight() * 0.4f), 4.0f);

        // Рамка
        g.setColour(CoheraUI::kTextDim.withAlpha(0.2f));
        g.drawRoundedRectangle(bounds, 6.0f, 1.0f);
    }

private:
    // Вспомогательная функция для рисования спектра с градиентной заливкой
    void drawSpectrum(juce::Graphics& g, float w, float h, const std::vector<float>& data, juce::Colour color, bool fill)
    {
        if (data.empty()) return;

        juce::Path p;
        float step = w / (data.size() - 1);

        // Строим Path
        p.startNewSubPath(0, h);
        for (size_t i = 0; i < data.size(); ++i) {
            float x = i * step;
            float y = h - (data[i] * h * 0.8f); // Оставляем место для меток
            if (i == 0)
                p.startNewSubPath(x, y);
            else
                p.lineTo(x, y);
        }
        p.lineTo(w, h);
        p.closeSubPath();

        if (fill) {
            // 🔥 ГРАДИЕНТНАЯ ЗАЛИВКА
            juce::ColourGradient fillGrad(
                color.withAlpha(0.4f), 0, 0,              // Верх: ярче
                color.withAlpha(0.0f), 0, h,              // Низ: прозрачный
                false);

            g.setGradientFill(fillGrad);
            g.fillPath(p);

            // Обводка (Sharp Line)
            g.setColour(color);
            g.strokePath(p, juce::PathStrokeType(1.5f));
        } else {
            g.setColour(color);
            g.strokePath(p, juce::PathStrokeType(2.0f));
        }
    }

    // Рисуем сетку в стиле радара
    void drawGrid(juce::Graphics& g, float w, float h) {
        // Тонкие линии
        g.setColour(CoheraUI::kTextDim.withAlpha(0.08f));

        // Вертикальные (частоты)
        float freqs[] = {50, 100, 200, 500, 1000, 2000, 5000, 10000};
        for(auto f : freqs) {
            float x = mapFreqToX(f, w);
            // Dash line
            float dash[] = {2.0f, 2.0f};
            juce::Line<float> line(x, 0, x, h);
            g.drawDashedLine(line, dash, 2);

            // Labels (только для основных)
            if (f == 100 || f == 1000 || f == 10000) {
                juce::String t = (f >= 1000) ? juce::String(f/1000) + "k" : juce::String((int)f);
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
    void drawSacredGeometry(juce::Graphics& g, float w, float h)
    {
        g.setColour(CoheraUI::kTextDim.withAlpha(0.03f)); // Едва заметно

        // Рисуем Золотую Спираль (приблизительно)
        juce::Path spiral;
        float x = w * 0.1f, y = h * 0.8f;
        float boxW = w * 0.8f;
        float boxH = h * 0.6f;

        spiral.startNewSubPath(x, y + boxH);

        // Несколько итераций золотого сечения
        for(int i = 0; i < 6; ++i) {
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

        g.drawEllipse(centerX - radius1, centerY - radius1, radius1 * 2, radius1 * 2, 1.0f);
        g.drawEllipse(centerX - radius2, centerY - radius2, radius2 * 2, radius2 * 2, 1.0f);
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
};
