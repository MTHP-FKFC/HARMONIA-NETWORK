#pragma once
#include "../../JuceHeader.h"

class AbstractVisualizer : public juce::Component, private juce::Timer
{
public:
    AbstractVisualizer(int fps = 60)
    {
        startTimerHz(fps);
    }

    ~AbstractVisualizer() override
    {
        stopTimer();
    }

    // Метод для передачи внешней энергии (RMS, Transients) в визуализатор
    virtual void setEnergyLevel(float level)
    {
        currentEnergy = level;
    }

    // Метод для обновления времени (для анимаций)
    void updateTime(float dt = 1.0f/60.0f)
    {
        time += dt;
    }

protected:
    void timerCallback() override
    {
        updateTime();
        updatePhysics();
        repaint();
    }

    // Чистая виртуальная функция: наследник ОБЯЗАН реализовать физику
    virtual void updatePhysics() = 0;

    float currentEnergy = 0.0f; // 0.0 .. 1.0
    float time = 0.0f;          // Глобальное время для анимаций
};
