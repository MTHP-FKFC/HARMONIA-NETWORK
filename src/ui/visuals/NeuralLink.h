#pragma once
#include "AbstractVisualizer.h"
#include "../CoheraLookAndFeel.h"

class NeuralLink : public AbstractVisualizer
{
public:
    NeuralLink(juce::AudioProcessorValueTreeState& apvts)
        : AbstractVisualizer(60), apvts(apvts) // Высокий FPS для плавной анимации
    {
        setInterceptsMouseClicks(false, false);
    }

    // Установка чувствительности (натяжение струны)
    void setTension(float t)
    {
        tension = juce::jlimit(0.1f, 3.0f, t);
    }

    // Установка режима для цвета
    void setMode(int mode)
    {
        currentMode = mode;
    }

protected:
    void updatePhysics() override
    {
        // Анимация фазы (бегущая волна)
        phase += 0.15f;
        if (phase > juce::MathConstants<float>::twoPi)
            phase -= juce::MathConstants<float>::twoPi;

        // Энергия влияет на амплитуду волн
        energyAmplitude = currentEnergy * 0.3f;
    }

    void paint(juce::Graphics& g) override
    {
        if (!isVisible() || getWidth() == 0 || getHeight() == 0) return;

        float w = (float)getWidth();
        float h = (float)getHeight();
        float centerX = w * 0.5f;

        // Определяем цвет "жилы" в зависимости от режима
        juce::Colour coreColor = (currentMode == 0) ? CoheraUI::kCyanNeon : CoheraUI::kOrangeNeon;

        // Рисуем 3 переплетающиеся нити (DNA style)
        for (int strandIndex = 0; strandIndex < 3; ++strandIndex)
        {
            juce::Path strand;
            strand.startNewSubPath(centerX, 0);

            // Сдвиг фазы для каждой нити
            float strandPhaseOffset = strandIndex * (juce::MathConstants<float>::pi / 1.5f);
            float baseAmplitude = w * 0.15f * tension; // Амплитуда зависит от Sens

            // Рисуем синусоиду вниз с высоким разрешением
            float step = 2.0f; // Пиксели
            for (float y = 0; y <= h; y += step)
            {
                // Формула: Sin(y + time + offset) + энергия
                float wave = std::sin(y * 0.03f - phase + strandPhaseOffset);
                wave += std::sin(y * 0.08f + phase * 2.0f + strandPhaseOffset) * 0.3f; // Гармоника

                float amplitude = baseAmplitude * (1.0f + energyAmplitude);
                float xOff = wave * amplitude;

                // Window function: плавное затухание к краям
                float window = std::sin((y / h) * juce::MathConstants<float>::pi);
                xOff *= window;

                strand.lineTo(centerX + xOff, y);
            }

            // Отрисовка нити с glow эффектом
            float alpha = (strandIndex == 1) ? 0.9f : 0.4f; // Центральная нить ярче
            float thickness = (strandIndex == 1) ? 2.0f : 1.0f;

            // Свечение (glow)
            g.setColour(coreColor.withAlpha(alpha * 0.3f));
            g.strokePath(strand, juce::PathStrokeType(thickness * 3.0f, juce::PathStrokeType::curved));

            // Ядро
            g.setColour(coreColor.withAlpha(alpha * (0.7f + currentEnergy * 0.3f)));
            g.strokePath(strand, juce::PathStrokeType(thickness, juce::PathStrokeType::curved));
        }

        // Добавляем пульсирующие узлы в местах пересечения
        if (currentEnergy > 0.2f)
        {
            drawPulseNodes(g, coreColor, w, h);
        }
    }

private:
    void drawPulseNodes(juce::Graphics& g, juce::Colour color, float w, float h)
    {
        float centerX = w * 0.5f;
        int nodeCount = 3;

        for (int i = 0; i < nodeCount; ++i)
        {
            float y = h * (i + 1) / (nodeCount + 1);
            float pulse = std::sin(time * 3.0f + i * 2.0f) * 0.5f + 0.5f;
            float nodeSize = (3.0f + pulse * 5.0f) * currentEnergy;

            g.setColour(color.withAlpha(pulse * currentEnergy * 0.6f));
            g.fillEllipse(centerX - nodeSize/2, y - nodeSize/2, nodeSize, nodeSize);
        }
    }

    juce::AudioProcessorValueTreeState& apvts;
    float phase = 0.0f;
    float tension = 1.0f; // От 0.1 до 3.0 (Sens)
    float energyAmplitude = 0.0f;
    int currentMode = 0;
};
