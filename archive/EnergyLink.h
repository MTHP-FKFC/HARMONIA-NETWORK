#pragma once

#include "../../JuceHeader.h"
#include "../CoheraLookAndFeel.h"

class EnergyLink : public juce::Component, private juce::Timer
{
public:
    EnergyLink() { startTimerHz(30); }
    ~EnergyLink() override { stopTimer(); }

    void timerCallback() override {
        phase += 0.1f;
        if(phase > juce::MathConstants<float>::twoPi) phase -= juce::MathConstants<float>::twoPi;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto h = (float)getHeight();
        auto w = (float)getWidth();
        auto cy = h * 0.5f;

        // Фон - центральная ось
        g.setColour(CoheraUI::kTextDim.withAlpha(0.1f));
        g.drawLine(w*0.5f, 0, w*0.5f, h, 2.0f); // Центральная ось

        // Активная пульсация (если есть связь)
        float glow = (std::sin(phase) + 1.0f) * 0.5f; // 0..1

        juce::Colour gradCol1 = CoheraUI::kOrangeNeon;
        juce::Colour gradCol2 = CoheraUI::kCyanNeon;

        juce::ColourGradient grad(
            gradCol1.withAlpha(0.3f + glow * 0.2f), 0, 0,
            gradCol2.withAlpha(0.3f + glow * 0.2f), w, h, true);

        g.setGradientFill(grad);

        // Рисуем треугольники (стрелочки) ▼▼▼
        float arrowSize = 6.0f;
        float spacing = 15.0f;
        int numArrows = (int)(h / spacing);

        for(int i=0; i<numArrows; ++i) {
            float y = i * spacing + (phase * 2.0f); // Движение вниз
            y = std::fmod(y, h); // Цикл

            juce::Path p;
            // Рисуем простой треугольник
            p.addTriangle(w*0.5f - arrowSize*0.5f, y - arrowSize*0.5f,
                         w*0.5f + arrowSize*0.5f, y - arrowSize*0.5f,
                         w*0.5f, y + arrowSize*0.5f);
            g.fillPath(p);
        }
    }

private:
    float phase = 0.0f;
};
