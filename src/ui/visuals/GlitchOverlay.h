#pragma once
#include "AbstractVisualizer.h"
#include "../CoheraLookAndFeel.h"

class GlitchOverlay : public AbstractVisualizer
{
public:
    GlitchOverlay() : AbstractVisualizer(60)
    {
        setInterceptsMouseClicks(false, false);
        // Предварительно выделяем память для глитч-координат
        glitchY.reserve(10);
        glitchH.reserve(10);
    }

protected:
    void updatePhysics() override
    {
        if (currentEnergy > 0.8f && random.nextFloat() > 0.7f)
            activeGlitchDuration = 5;

        if (activeGlitchDuration > 0)
            --activeGlitchDuration;
            
        // Вычисляем глитч-координаты здесь, а не в paint()
        if (currentEnergy > 0.9f || activeGlitchDuration > 0) {
            glitchY.clear();
            glitchH.clear();
            
            int numStrips = 5;
            for (int i = 0; i < numStrips; ++i) {
                glitchY.push_back(random.nextFloat() * getHeight());
                glitchH.push_back(random.nextFloat() * 20.0f + 2.0f);
            }
        }
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
            // Используем предвычисленные координаты из updatePhysics()
            for (int i = 0; i < (int)glitchY.size() && i < (int)glitchH.size(); ++i)
            {
                float y = glitchY[i];
                float stripH = glitchH[i];

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
    
    // Кэшированные координаты глитчей для оптимизации
    std::vector<float> glitchY;
    std::vector<float> glitchH;
};
