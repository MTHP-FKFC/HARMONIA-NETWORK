#pragma once
#include "AbstractVisualizer.h"
#include "../CoheraLookAndFeel.h"
#include <cmath>

class BioScanner : public AbstractVisualizer
{
public:
    BioScanner() : AbstractVisualizer(15) // Уменьшили FPS для производительности
    {
        setInterceptsMouseClicks(false, false);
    }

    // Метод для установки уровня энергии (RMS)
    void setEnergyLevel(float energy)
    {
        currentEnergy = energy;
    }

protected:
    void updatePhysics() override
    {
        scanPhase += 0.03f;
        if (scanPhase > juce::MathConstants<float>::twoPi)
            scanPhase -= juce::MathConstants<float>::twoPi;
    }

    void paint(juce::Graphics& g) override
    {
        float w = (float)getWidth();
        float h = (float)getHeight();
        float posNormal = (std::sin(scanPhase) + 1.0f) * 0.5f;
        float x = posNormal * w;

        // Простой яркий цвет вместо сложного градиента
        float brightness = juce::jlimit(0.3f, 1.0f, currentEnergy + 0.4f);
        juce::Colour scanColor = CoheraUI::kCyanNeon.withAlpha(brightness);

        // Простой прямоугольник вместо градиента
        g.setColour(scanColor);
        g.fillRect(x - 1.5f, 0.0f, 3.0f, h);

        // Убираем текст и линии для производительности
        // Только основной луч остается
    }

private:
    float scanPhase = 0.0f;
    float currentEnergy = 0.5f; // Default energy level
};
