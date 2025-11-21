#pragma once

#include "AbstractVisualizer.h"
#include "../CoheraLookAndFeel.h"

struct GlitchBlock {
    juce::Rectangle<float> area;
    int lifeTime;
    juce::Colour color;
    int type; // 0 = Fill, 1 = Outline, 2 = Offset
};

class DigitalArtifacts : public AbstractVisualizer
{
public:
    DigitalArtifacts() : AbstractVisualizer(30) // 30 FPS достаточно
    {
        setInterceptsMouseClicks(false, false);
    }

protected:
    void updatePhysics() override
    {
        // 1. Удаляем мертвые блоки
        for (auto it = blocks.begin(); it != blocks.end();) {
            it->lifeTime--;
            if (it->lifeTime <= 0) it = blocks.erase(it);
            else ++it;
        }

        // 2. Спавним новые (вероятность зависит от энергии)
        // Базовая вероятность низкая (фоновые глюки) + всплеск от транзиентов
        float chance = 0.05f + (currentEnergy * 0.3f);

        if (random.nextFloat() < chance)
        {
            float w = getWidth();
            float h = getHeight();

            // Размер блока (от мелкого до среднего)
            float bw = random.nextFloat() * 100.0f + 10.0f;
            float bh = random.nextFloat() * 20.0f + 2.0f; // Полоски
            float x = random.nextFloat() * (w - bw);
            float y = random.nextFloat() * (h - bh);

            juce::Colour col;
            if (random.nextFloat() > 0.5f) col = CoheraUI::kCyanNeon;
            else col = CoheraUI::kOrangeNeon;

            blocks.push_back({
                juce::Rectangle<float>(x, y, bw, bh),
                random.nextInt(5) + 2, // Живет 2-7 кадров
                col,
                random.nextInt(3)
            });
        }
    }

    void paint(juce::Graphics& g) override
    {
        for (const auto& b : blocks)
        {
            float alpha = (float)b.lifeTime / 10.0f * 0.2f; // Очень прозрачно
            g.setColour(b.color.withAlpha(alpha));

            if (b.type == 0) {
                g.fillRect(b.area);
            }
            else if (b.type == 1) {
                g.drawRect(b.area, 1.0f);
            }
            else if (b.type == 2) {
                // Эффект сдвига (рисуем полоски)
                for(float i=0; i<b.area.getHeight(); i+=2) {
                    g.fillRect(b.area.getX(), b.area.getY() + i, b.area.getWidth(), 1.0f);
                }
            }
        }
    }

private:
    std::vector<GlitchBlock> blocks;
    juce::Random random;
};
