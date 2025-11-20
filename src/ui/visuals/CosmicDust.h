#pragma once
#include "AbstractVisualizer.h"
#include "../CoheraLookAndFeel.h"
#include <vector>

struct Particle {
    float x, y;         // Позиция (0..1 normalized)
    float vx, vy;       // Скорость
    float size;         // Размер
    float depth;        // Глубина (0.1 - даль, 1.0 - передний план)
    float baseAlpha;    // Базовая прозрачность
    float life;         // Время жизни для эффектов
};

class CosmicDust : public AbstractVisualizer
{
public:
    CosmicDust() : AbstractVisualizer(30) // 30 FPS достаточно для фона
    {
        setInterceptsMouseClicks(false, false); // Пропускаем клики сквозь пыль
        spawnParticles(80); // Больше частиц для богатства
    }

    void setEnergyLevel(float level) override
    {
        AbstractVisualizer::setEnergyLevel(level);
        // Энергия влияет на поведение частиц
    }

protected:
    void updatePhysics() override
    {
        for (auto& p : particles)
        {
            // 1. Базовое движение (дрейф с параллаксом)
            p.x += p.vx * p.depth * (1.0f + currentEnergy * 0.5f); // Энергия ускоряет движение
            p.y += p.vy * p.depth * (1.0f + currentEnergy * 0.5f);

            // 2. Реакция на энергию (Shockwave effect)
            if (currentEnergy > 0.1f) {
                float shake = (random.nextFloat() - 0.5f) * 0.02f * currentEnergy * p.depth;
                p.x += shake;
                p.y += shake;

                // Энергия делает частицы ярче и больше
                p.size *= (1.0f + currentEnergy * 0.1f);
            }

            // 3. Wrap around (бесконечный экран)
            if (p.x > 1.0f) p.x = 0.0f; else if (p.x < 0.0f) p.x = 1.0f;
            if (p.y > 1.0f) p.y = 0.0f; else if (p.y < 0.0f) p.y = 1.0f;

            // 4. Размер возвращается к норме
            p.size = juce::jlimit(0.5f, 3.0f, p.size * 0.98f); // Плавное затухание
        }
    }

    void paint(juce::Graphics& g) override
    {
        // Рисуем только если компонент виден
        if (!isVisible() || getWidth() == 0) return;

        for (const auto& p : particles)
        {
            // Параллакс: дальние частицы темнее и меньше
            float alpha = p.baseAlpha * p.depth * (0.3f + currentEnergy * 0.7f); // Ярче от звука
            float size = p.size * p.depth;

            // Цвет зависит от глубины: дальние - синие, ближние - оранжевые
            juce::Colour particleColor = (p.depth < 0.5f) ?
                CoheraUI::kCyanNeon.withAlpha(alpha) :
                CoheraUI::kOrangeNeon.withAlpha(alpha);

            g.setColour(particleColor);

            float drawX = p.x * getWidth();
            float drawY = p.y * getHeight();

            g.fillEllipse(drawX - size/2, drawY - size/2, size, size);
        }
    }

private:
    void spawnParticles(int count)
    {
        random.setSeed(42); // Фиксированное зерно для консистентности

        for (int i = 0; i < count; ++i) {
            particles.push_back({
                random.nextFloat(), random.nextFloat(),                 // Pos
                (random.nextFloat() - 0.5f) * 0.001f,                   // VX (медленное движение)
                (random.nextFloat() - 0.5f) * 0.001f,                   // VY
                random.nextFloat() * 2.0f + 0.5f,                       // Size
                random.nextFloat() * 0.9f + 0.1f,                       // Depth
                random.nextFloat() * 0.4f + 0.1f,                       // Alpha
                1.0f                                                    // Life
            });
        }
    }

    std::vector<Particle> particles;
    juce::Random random;
};
