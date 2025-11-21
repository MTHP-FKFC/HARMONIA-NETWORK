#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "../CoheraLookAndFeel.h"

namespace CoheraUI {

    // ============================================================================
    // 1. СТРАТЕГИЯ ФИЗИКИ (The Strategy Pattern)
    // ============================================================================
    class ReactionPhysics
    {
    public:
        virtual ~ReactionPhysics() = default;

        // Возвращает новое визуальное значение (0..1) на основе целевого значения
        virtual float update(float targetValue) = 0;

        // Возвращает цвет для текущего состояния (можно менять цвет от интенсивности)
        virtual juce::Colour getColor(float intensity, juce::Colour baseColor) {
            return baseColor.withAlpha(intensity);
        }
    };

    // --- Реализация: Лампа (Для SENS) ---
    // Быстрая атака, экспоненциальное затухание (тепловая инерция)
    class LampPhysics : public ReactionPhysics
    {
    public:
        float update(float targetValue) override
        {
            // Если пришел новый пик - мгновенный нагрев (с небольшой инерцией)
            if (targetValue > currentTemp) {
                currentTemp = currentTemp * 0.6f + targetValue * 0.4f;
            }
            // Остывание
            else {
                currentTemp *= coolingRate;
            }

            // Клиппинг шума
            if (currentTemp < 0.01f) currentTemp = 0.0f;
            return currentTemp;
        }

    private:
        float currentTemp = 0.0f;
        const float coolingRate = 0.92f; // Скорость остывания нити
    };

    // --- Реализация: Плазма (Для DEPTH) ---
    // Вязкое движение + LFO (дыхание)
    class PlasmaPhysics : public ReactionPhysics
    {
    public:
        float update(float targetValue) override
        {
            // Глобальное время для LFO
            float time = (float)juce::Time::getMillisecondCounter() * 0.002f;

            // 1. Сглаживание входа (Вязкость)
            smoothedValue = smoothedValue * 0.95f + targetValue * 0.05f;

            // 2. Дыхание (LFO), амплитуда зависит от силы сигнала
            // Если сигнала нет - не дышим.
            float breath = 0.1f * std::sin(time);
            float result = smoothedValue + (smoothedValue * breath);

            return juce::jlimit(0.0f, 1.0f, result);
        }

        // Плазма меняет оттенок при высокой интенсивности (белеет)
        juce::Colour getColor(float intensity, juce::Colour baseColor) override {
            if (intensity > 0.8f)
                return baseColor.interpolatedWith(juce::Colours::white, (intensity - 0.8f) * 2.0f);
            return baseColor;
        }

    private:
        float smoothedValue = 0.0f;
    };

    // ============================================================================
    // 2. УМНАЯ РУЧКА (The Component)
    // ============================================================================
    class SmartReactorKnob : public juce::Slider, private juce::Timer
    {
    public:
        // Конструктор принимает стратегию физики через unique_ptr
        SmartReactorKnob(std::unique_ptr<ReactionPhysics> physicsModel, juce::Colour color)
            : physics(std::move(physicsModel)), baseColor(color)
        {
            // Настройки слайдера
            setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
            setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
            setColour(juce::Slider::thumbColourId, baseColor); // Для LookAndFeel

            startTimerHz(60); // 60 FPS UI
        }

        ~SmartReactorKnob() override { stopTimer(); }

        // Подключение источника данных (Dependency Injection)
        void setDataSource(std::function<float()> source) {
            dataSource = source;
        }

        // Смена цвета на лету (например, при смене режима сети)
        void setBaseColor(juce::Colour newColor) {
            baseColor = newColor;
            setColour(juce::Slider::thumbColourId, baseColor);
        }

    private:
        void timerCallback() override
        {
            if (!physics || !dataSource) return;

            // 1. Получаем данные из процессора
            float rawInput = dataSource();

            // 2. Считаем физику
            visualLevel = physics->update(rawInput);

            // 3. Оптимизация отрисовки (не перерисовываем, если изменений нет)
            if (std::abs(visualLevel - lastRepaintLevel) > 0.002f) {
                lastRepaintLevel = visualLevel;
                repaint();
            }
        }

        void paint(juce::Graphics& g) override
        {
            // 1. Рисуем базу (через наш крутой LookAndFeel)
            // Используем базовую реализацию Slider для рисования
            juce::Slider::paint(g);

            // 2. Рисуем Реакцию (Поверх базы)
            if (visualLevel > 0.01f)
            {
                drawReactorGlow(g);
            }
        }

        void drawReactorGlow(juce::Graphics& g)
        {
            auto center = getLocalBounds().getCentre().toFloat();
            float radius = juce::jmin(getWidth(), getHeight()) / 2.0f * 0.65f;

            // Получаем динамический цвет от физики
            juce::Colour glowColor = physics->getColor(visualLevel, baseColor);
            float alpha = visualLevel; // Интенсивность

            // A. Ядро (Core)
            juce::ColourGradient coreGrad(
                glowColor.withAlpha(alpha), center.x, center.y,
                juce::Colours::transparentBlack, center.x, center.y + radius, true);

            g.setGradientFill(coreGrad);
            g.fillEllipse(center.x - radius, center.y - radius, radius * 2, radius * 2);

            // B. Ободок (Rim) - становится ярче и толще от сигнала
            float rimThickness = 1.5f + alpha * 2.0f;
            g.setColour(glowColor.brighter(0.8f).withAlpha(alpha * 0.7f));

            // Рисуем чуть внутри ручки
            float rimRadius = radius - 2.0f;
            g.drawEllipse(center.x - rimRadius, center.y - rimRadius, rimRadius * 2, rimRadius * 2, rimThickness);
        }

        std::unique_ptr<ReactionPhysics> physics;
        std::function<float()> dataSource;
        juce::Colour baseColor;

        float visualLevel = 0.0f;
        float lastRepaintLevel = 0.0f;
    };

} // namespace CoheraUI
