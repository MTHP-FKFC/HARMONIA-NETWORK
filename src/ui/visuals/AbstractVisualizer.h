#pragma once
#include "../../JuceHeader.h"

class AbstractVisualizer : public juce::Component, private juce::Timer
{
public:
    AbstractVisualizer(int fps = 60) : targetFPS(fps)
    {
        // НЕ запускаем таймер в конструкторе - только после добавления в дерево
        // startTimerHz(fps);
    }

    ~AbstractVisualizer() override
    {
        stopTimer();
    }

    void parentHierarchyChanged() override
    {
        // Запускаем таймер только когда компонент добавлен в дерево
        if (getParentComponent() != nullptr && !isTimerRunning())
        {
            startTimerHz(targetFPS);
        }
        else if (getParentComponent() == nullptr)
        {
            stopTimer();
        }
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
        // Безопасность: проверяем, что компонент видим и имеет размер
        if (!isVisible() || getWidth() <= 0 || getHeight() <= 0)
            return;

        updateTime();
        updatePhysics();
        repaint();
    }

    // Чистая виртуальная функция: наследник ОБЯЗАН реализовать физику
    virtual void updatePhysics() = 0;

    float currentEnergy = 0.0f; // 0.0 .. 1.0
    float time = 0.0f;          // Глобальное время для анимаций
    int targetFPS = 60;         // Целевой FPS для таймера
};
