#pragma once
#include "../../JuceHeader.h"

class SpectrumVisor : public juce::Component
{
public:
    SpectrumVisor() {}

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds();
        
        // Фон экрана (глубокий черный/синий)
        g.setColour(juce::Colour::fromRGB(10, 12, 15));
        g.fillRect(area);
        
        // Сетка (Grid)
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        // Вертикальные линии (частоты)
        for (float f = 100.0f; f < 20000.0f; f *= 10.0f) {
            float x = mapLogFreq(f, area.getWidth());
            g.drawVerticalLine((int)x, 0, (float)area.getHeight());
        }
        // Горизонтальные (dB)
        g.drawHorizontalLine(area.getHeight() / 2, 0, (float)area.getWidth());

        // Текст заглушка
        g.setColour(juce::Colours::grey);
        g.drawText("SPECTRAL INTERACTION VIEW", area, juce::Justification::centred);
    }
    
    // Хелпер для логарифмической шкалы (грубый)
    float mapLogFreq(float freq, float width) {
        return width * (std::log10(freq / 20.0f) / std::log10(20000.0f / 20.0f));
    }
};