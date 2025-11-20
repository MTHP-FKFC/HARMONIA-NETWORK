#pragma once
#include "../JuceHeader.h"

class CoheraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CoheraLookAndFeel()
    {
        // Глобальные цвета
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour::fromRGB(20, 22, 26));
        setColour(juce::Label::textColourId, juce::Colours::lightgrey.withAlpha(0.8f));
        
        // Шрифты (можно подключить свои, пока дефолт)
        // setDefaultSansSerifTypefaceName("Verdana"); 
    }

    // Рисуем крутую ручку
    void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height, float sliderPos,
                          const float rotaryStartAngle, const float rotaryEndAngle, juce::Slider& slider) override
    {
        auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat();
        auto center = bounds.getCentre();
        float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
        
        // 1. ЦВЕТОВАЯ КОДИРОВКА
        // Определяем цвет по имени компонента (хак, но удобный)
        // Orange (Tube) по дефолту
        juce::Colour mainColor = juce::Colour::fromRGB(255, 140, 0); 
        
        if (slider.getName().containsIgnoreCase("Net") || slider.getName().containsIgnoreCase("Depth")) {
            // Mint/Cyan (Network)
            mainColor = juce::Colour::fromRGB(0, 220, 160); 
        }
        
        // 2. ТЕЛО РУЧКИ (Background)
        float knobRadius = radius * 0.75f;
        g.setColour(juce::Colour::fromRGB(30, 32, 36)); // Темно-серый мат
        g.fillEllipse(center.x - knobRadius, center.y - knobRadius, knobRadius * 2, knobRadius * 2);
        
        // Обводка тела
        g.setColour(juce::Colours::black.withAlpha(0.5f));
        g.drawEllipse(center.x - knobRadius, center.y - knobRadius, knobRadius * 2, knobRadius * 2, 2.0f);

        // 3. ИНДИКАТОР (Arc)
        // Рисуем дугу вокруг ручки
        juce::Path arcPath;
        float arcRadius = radius * 0.85f;
        float toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        
        // Фон дуги (серый)
        juce::Path backgroundArc;
        backgroundArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        g.strokePath(backgroundArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        // Активная дуга (цветная)
        if (slider.isEnabled())
        {
            juce::Path valueArc;
            valueArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);
            
            // Glow effect (свечение)
            g.setColour(mainColor.withAlpha(0.3f));
            g.strokePath(valueArc, juce::PathStrokeType(8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            
            // Core line
            g.setColour(mainColor);
            g.strokePath(valueArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // 4. УКАЗАТЕЛЬ (Dot/Line на самой ручке)
        float dotRadius = 3.0f;
        float dotDist = knobRadius * 0.6f;
        float dotX = center.x + std::cos(toAngle - juce::MathConstants<float>::halfPi) * dotDist;
        float dotY = center.y + std::sin(toAngle - juce::MathConstants<float>::halfPi) * dotDist;
        
        g.setColour(slider.isEnabled() ? juce::Colours::white : juce::Colours::grey);
        g.fillEllipse(dotX - dotRadius, dotY - dotRadius, dotRadius * 2, dotRadius * 2);
    }
    
    // Также стилизуем ComboBox (выпадающие списки)
    void drawComboBox(juce::Graphics& g, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH,
                      juce::ComboBox& box) override
    {
        auto cornerSize = box.findParentComponentOfClass<juce::GroupComponent>() ? 0.0f : 3.0f;
        juce::Rectangle<int> boxBounds(0, 0, width, height);

        g.setColour(juce::Colour::fromRGB(30, 32, 36));
        g.fillRoundedRectangle(boxBounds.toFloat(), cornerSize);

        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawRoundedRectangle(boxBounds.toFloat().reduced(0.5f), cornerSize, 1.0f);
        
        // Стрелочка
        juce::Path p;
        p.addTriangle(width - 20, height * 0.5f - 2, width - 10, height * 0.5f - 2, width - 15, height * 0.5f + 3);
        g.setColour(juce::Colours::grey);
        g.fillPath(p);
    }
};