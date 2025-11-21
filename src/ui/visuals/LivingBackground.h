#pragma once

#include "AbstractVisualizer.h"
#include "DigitalArtifacts.h"
#include "AnalogNoise.h"
#include "../CoheraLookAndFeel.h"

class LivingBackground : public juce::Component
{
public:
    LivingBackground()
    {
        // Порядок слоев важен!
        addAndMakeVisible(artifacts);
        addAndMakeVisible(noise);
    }

    void resized() override
    {
        artifacts.setBounds(getLocalBounds());
        noise.setBounds(getLocalBounds());
    }

    // Прокидываем энергию во все слои
    void setEnergyLevel(float level)
    {
        artifacts.setEnergyLevel(level);
        noise.setEnergyLevel(level);
        currentEnergy = level; // Для собственной отрисовки
        repaint(); // Перерисовываем градиент
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();

        // 1. Базовый цвет (Deep Dark)
        g.fillAll(CoheraUI::kBackground);

        // 2. "Дышащая" Виньетка
        // Центр чуть смещается и пульсирует от энергии
        float breathe = 1.0f + currentEnergy * 0.2f;

        juce::ColourGradient vignette(
            CoheraUI::kBackground.brighter(0.05f * breathe),
            area.getCentreX(), area.getCentreY(),
            juce::Colours::black,
            0, 0, true);

        g.setGradientFill(vignette);
        g.fillAll();

        // 3. Subtly glowing grid lines (Optional texture)
        // Очень тонкие линии на фоне
        g.setColour(juce::Colours::white.withAlpha(0.02f));
        for(float x=0; x<getWidth(); x+=40) g.drawVerticalLine((int)x, 0.0f, (float)getHeight());
        for(float y=0; y<getHeight(); y+=40) g.drawHorizontalLine((int)y, 0.0f, (float)getWidth());
    }

private:
    DigitalArtifacts artifacts;
    AnalogNoise noise;
    float currentEnergy = 0.0f;
};
