#pragma once
#include "AbstractVisualizer.h"
#include "../CoheraLookAndFeel.h"
#include <cmath>

class BioScanner : public AbstractVisualizer
{
public:
    BioScanner() : AbstractVisualizer(40)
    {
        setInterceptsMouseClicks(false, false);
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

        float brightness = juce::jlimit(0.2f, 1.0f, currentEnergy + 0.25f);
        juce::Colour scanColor = CoheraUI::kCyanNeon.withAlpha(0.2f + brightness * 0.6f);

        juce::ColourGradient beamGrad(
            scanColor.withAlpha(0.0f), x, 0,
            scanColor.withAlpha(0.0f), x, h, false);
        beamGrad.addColour(0.5, scanColor.withAlpha(0.6f));
        g.setGradientFill(beamGrad);
        g.fillRect(x - 1.0f, 0.0f, 3.0f, h);

        g.setFont(juce::Font("Courier New", 10.0f, juce::Font::plain));
        g.setColour(scanColor);
        juce::String dataStr = juce::String::formatted("%03dHz", (int)(posNormal * 20000));
        g.drawText(dataStr, (int)x + 5, (int)(h / 2), 60, 10, juce::Justification::left);

        for (int i = 0; i < 3; ++i)
        {
            float fx = x - (i + 1) * 12.0f;
            g.setColour(scanColor.withAlpha(0.1f + (0.2f * (i + 1))));
            g.drawLine(fx, 0.0f, fx, h, 1.0f);
        }
    }

private:
    float scanPhase = 0.0f;
};
