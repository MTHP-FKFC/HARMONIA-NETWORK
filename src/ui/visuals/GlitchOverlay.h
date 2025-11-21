#pragma once
#include "AbstractVisualizer.h"
#include "../CoheraLookAndFeel.h"

class GlitchOverlay : public AbstractVisualizer
{
public:
    GlitchOverlay() : AbstractVisualizer(60)
    {
        setInterceptsMouseClicks(false, false);
    }

protected:
    void updatePhysics() override
    {
        if (currentEnergy > 0.8f && random.nextFloat() > 0.7f)
            activeGlitchDuration = 5;

        if (activeGlitchDuration > 0)
            --activeGlitchDuration;
    }

    void paint(juce::Graphics& g) override
    {
        if (activeGlitchDuration <= 0 && currentEnergy < 0.9f)
            return;

        float w = (float)getWidth();
        float h = (float)getHeight();
        float intensity = (currentEnergy * 0.5f) + (activeGlitchDuration > 0 ? 0.5f : 0.0f);

        float shift = intensity * 10.0f;

        if (intensity > 0.1f)
        {
            int numStrips = 5;
            for (int i = 0; i < numStrips; ++i)
            {
                float y = random.nextFloat() * h;
                float stripH = random.nextFloat() * 20.0f + 2.0f;

                g.setColour(juce::Colours::cyan.withAlpha(0.3f * intensity));
                g.fillRect(-shift, y, w, stripH);

                g.setColour(juce::Colours::red.withAlpha(0.3f * intensity));
                g.fillRect(shift, y, w, stripH);
            }
        }

        if (activeGlitchDuration > 0)
        {
            g.setColour(juce::Colours::white.withAlpha(0.8f));
            float tearY = random.nextFloat() * h;
            g.fillRect(0.0f, tearY, w, 2.0f);
            g.fillRect(0.0f, tearY + 5.0f, w * 0.5f, 1.0f);
        }
    }

private:
    int activeGlitchDuration = 0;
    juce::Random random;
};
