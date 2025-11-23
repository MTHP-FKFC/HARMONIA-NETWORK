#pragma once
#include "../../JuceHeader.h"
#include "../CoheraLookAndFeel.h"

class ReactorKnob : public juce::Slider, private juce::Timer
{
public:
    ReactorKnob()
        : rmsSource([](){ return 0.0f; })
    {
        // Таймер запустится в visibilityChanged()
    }

    ReactorKnob(std::function<float()> rmsGetter)
        : rmsSource(rmsGetter)
    {
        // Таймер запустится в visibilityChanged()
    }

    void setRMSGetter(std::function<float()> getter) {
        rmsSource = getter;
    }

    ~ReactorKnob() override { stopTimer(); }
    
    void visibilityChanged() override {
        if (isVisible()) 
            startTimerHz(30); // 30 FPS достаточно для ручки
        else 
            stopTimer();
    }

    void timerCallback() override
    {
        float target = rmsSource();
        // Idle optimization - не обновляем если ничего не меняется
        float diff = std::abs(currentLevel - target);
        if (diff < 0.001f && std::abs(currentLevel - lastPaintedLevel) < 0.001f) return;

        // Сглаживаем движение света
        currentLevel = currentLevel * 0.8f + target * 0.2f;

        // Перерисовываем только если изменилось значимо
        if (std::abs(currentLevel - lastPaintedLevel) > 0.01f) {
            lastPaintedLevel = currentLevel;
            repaint();
        }
    }

    void paint(juce::Graphics& g) override
    {
        // Рисуем базовую ручку через родительский класс
        Slider::paint(g);

        // --- ДОБАВЛЯЕМ МАГИЮ ПОВЕРХ ---
        auto center = getLocalBounds().getCentre().toFloat();
        float radius = juce::jmin(getWidth(), getHeight()) / 2.0f * 0.65f; // Размер тела ручки

        if (currentLevel > 0.01f)
        {
            // Рисуем "Ядро реактора" в центре
            g.setColour(CoheraUI::kOrangeNeon.withAlpha(currentLevel * 0.8f)); // Яркость от звука!

            // Эффект: Центральное пятно
            juce::ColourGradient glow(
                CoheraUI::kOrangeNeon.withAlpha(currentLevel), center.x, center.y,
                juce::Colours::transparentBlack, center.x, center.y + radius, true);

            g.setGradientFill(glow);
            g.fillEllipse(center.x - radius, center.y - radius, radius * 2, radius * 2);

            // Эффект: Электрический ободок
            g.setColour(juce::Colours::white.withAlpha(currentLevel * 0.6f));
            g.drawEllipse(center.x - radius + 2, center.y - radius + 2, radius * 2 - 4, radius * 2 - 4, 2.0f);
        }
    }

private:
    std::function<float()> rmsSource;
    float currentLevel = 0.0f;
    float lastPaintedLevel = 0.0f;
};
