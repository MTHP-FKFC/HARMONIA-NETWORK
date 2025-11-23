/*
  ==============================================================================
    BioScanner.h
    Визуализация теплового ядра: От льда к плазме.
  ==============================================================================
*/

#pragma once
#include "AbstractVisualizer.h"
#include "../../PluginProcessor.h"

class BioScanner : public AbstractVisualizer
{
public:
    BioScanner(CoheraSaturatorAudioProcessor& p) : AbstractVisualizer(60), audioProcessor(p)
    {
        // Таймер запустится автоматически через parentHierarchyChanged
    }

    void updatePhysics() override
    {
        // Обновляем термальную физику на основе процессора
        // Температура уже рассчитывается в processBlock, здесь можно добавить анимации
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float tempNormal = audioProcessor.getNormalizedTemperature(); // 0.0 to 1.0
        
        // --- 1. МАГИЯ ЦВЕТА (Color Morphing) ---
        // Blue (Hue 0.6) -> Red (Hue 0.0)
        float hue = juce::jmap(tempNormal, 0.0f, 1.0f, 0.6f, 0.0f);
        
        // Saturation растет с нагревом (ярче)
        float saturation = juce::jmap(tempNormal, 0.5f, 1.0f, 0.5f, 1.0f);
        
        juce::Colour coreColor = juce::Colour::fromHSV(hue, saturation, 0.9f, 1.0f);
        
        // --- 2. ГЛИТЧ-ЭФФЕКТ (При перегреве > 80%) ---
        float shakeX = 0.0f;
        float shakeY = 0.0f;
        
        if (tempNormal > 0.8f)
        {
            juce::Random& r = juce::Random::getSystemRandom();
            float chaosLevel = (tempNormal - 0.8f) * 50.0f; // Сила тряски
            shakeX = r.nextFloat() * chaosLevel - (chaosLevel / 2.0f);
            shakeY = r.nextFloat() * chaosLevel - (chaosLevel / 2.0f);
        }

        // Применяем смещение
        auto center = bounds.getCentre().translated(shakeX, shakeY);
        float radius = (juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f) * 0.8f;

        // "Пульсация" радиуса от нагрева
        float pulse = std::sin(juce::Time::getMillisecondCounter() / 200.0f) * (5.0f * tempNormal);
        radius += pulse;

        // --- 3. ОТРИСОВКА ЯДРА ---
        
        // Градиентная заливка (свечение изнутри)
        juce::ColourGradient grad(
            coreColor.withAlpha(0.9f), center.x, center.y,
            coreColor.withAlpha(0.0f), center.x, center.y - radius * 1.5f,
            true // Radial
        );
        g.setGradientFill(grad);
        g.fillEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f);

        // Отрисовка "Техно-кольца" вокруг
        g.setColour(coreColor);
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 2.0f);

        // --- 4. ТЕКСТ ДАННЫХ ---
        g.setFont(juce::Font("Consolas", 14.0f, juce::Font::bold));
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        
        juce::String tempText = juce::String(audioProcessor.getCurrentTemperature(), 1) + " C";
        
        // Если перегрев - пишем WARNING
        if (tempNormal > 0.9f) {
            if (juce::Time::getMillisecondCounter() % 200 < 100) // Мигание
                g.drawText("CRITICAL TEMP", bounds, juce::Justification::centredTop);
        }

        g.drawText(tempText, bounds, juce::Justification::centred);
    }

    void resized() override {}

private:
    CoheraSaturatorAudioProcessor& audioProcessor;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(BioScanner)
};
