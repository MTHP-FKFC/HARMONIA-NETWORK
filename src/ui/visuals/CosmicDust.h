#pragma once
#include "AbstractVisualizer.h"
#include "../CoheraLookAndFeel.h"

struct DustParticle {
    float x, y, speed, size, alpha;
};

class CosmicDust : public AbstractVisualizer
{
public:
    CosmicDust() : AbstractVisualizer(30) // 30 FPS достаточно для фона
    {
        setInterceptsMouseClicks(false, false);
        particles.resize(100); // 100 частиц достаточно для атмосферы
        
        // Init
        for(auto& p : particles) resetParticle(p);
    }

protected:
    void updatePhysics() override
    {
        float w = (float)getWidth();
        float h = (float)getHeight();
        
        for(auto& p : particles) {
            p.y += p.speed;
            if (p.y > h) resetParticle(p);
        }
    }

    void paint(juce::Graphics& g) override
    {
        g.setColour(juce::Colours::white.withAlpha(0.15f)); // Общий цвет
        
        // Рисуем точками (быстро)
        for(const auto& p : particles) {
            g.fillEllipse(p.x, p.y, p.size, p.size);
        }
    }

private:
    void resetParticle(DustParticle& p) {
        p.x = random.nextFloat() * getWidth();
        p.y = -10.0f;
        p.speed = 0.5f + random.nextFloat() * 1.5f;
        p.size = 1.0f + random.nextFloat() * 2.0f;
    }

    std::vector<DustParticle> particles;
    juce::Random random;
};
