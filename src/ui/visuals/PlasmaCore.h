#pragma once

#include "AbstractVisualizer.h"
#include "../CoheraLookAndFeel.h"
#include <cmath>
#include <vector>

// Структура данных для передачи состояния из Audio Thread в UI Thread
struct PlasmaState {
    float driveLevel;    // 0..1 (Искажение / Турбулентность)
    float leftSignal;    // 0..1 (Левая нить)
    float rightSignal;   // 0..1 (Правая нить)
    float netModulation; // 0..1 (Сетевое вмешательство / Глитч)
    float globalHeat;    // 0..1 (Перегруз / Белая вспышка)
};

class PlasmaCore : public AbstractVisualizer
{
public:
    PlasmaCore() : AbstractVisualizer(60) // 60 FPS для гладкой физики
    {
        setInterceptsMouseClicks(false, false);
    }

    // Метод для обновления данных (вызывать из timerCallback редактора)
    void updateState(const PlasmaState& newState) {
        // Сглаживаем входящие данные для инерции физики
        state.driveLevel    = state.driveLevel * 0.9f + newState.driveLevel * 0.1f;
        state.leftSignal    = state.leftSignal * 0.8f + newState.leftSignal * 0.2f;
        state.rightSignal   = state.rightSignal * 0.8f + newState.rightSignal * 0.2f;
        state.netModulation = state.netModulation * 0.7f + newState.netModulation * 0.3f;

        // Heat (вспышка) имеет мгновенную атаку и медленный спад
        if (newState.globalHeat > state.globalHeat)
            state.globalHeat = newState.globalHeat;
        else
            state.globalHeat *= 0.92f; // Остывание
    }

protected:
    // === ФИЗИЧЕСКИЙ ДВИЖОК ===
    void updatePhysics() override
    {
        time += 0.05f * (1.0f + state.driveLevel * 2.0f); // Драйв ускоряет поток

        // Генерация турбулентности
        // Если есть сетевая модуляция, добавляем высокочастотный джиттер (Глитч)
        jitter = (random.nextFloat() - 0.5f) * state.netModulation * 20.0f;
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        float w = bounds.getWidth();
        float h = bounds.getHeight();
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        w_cache = w; // Cache for physics

        // 1. СТЕКЛЯННЫЙ СОСУД (CONTAINMENT VESSEL)
        drawGlassContainer(g, bounds);

        // Клиппинг, чтобы плазма не вылезала за рамки стекла
        g.saveState();
        g.reduceClipRegion(bounds.reduced(4.0f).toNearestInt());

        // 2. ЯДРО ПЛАЗМЫ (3 НИТИ)
        // Рисуем 3 нити: L, R и Центральная (Сумма)

        // A. Left Strand (Cyan/Blue)
        float leftSig = std::max(state.leftSignal, 0.35f); // Минимальная видимость
        drawPlasmaStrand(g, cx - 4.0f, bounds.getBottom(), leftSig,
                         CoheraUI::kCyanNeon, -1.0f);

        // B. Right Strand (Magenta/Orange)
        float rightSig = std::max(state.rightSignal, 0.35f); // Минимальная видимость
        drawPlasmaStrand(g, cx + 4.0f, bounds.getBottom(), rightSig,
                         CoheraUI::kOrangeNeon, 1.0f);

        // C. Central Core (White/Gold - Fusion)
        // Всегда виден, но ярче при сигнале
        float centralCoreIntensity = std::max((state.leftSignal + state.rightSignal) * 0.5f, 0.2f);
        drawCoreStrand(g, cx, bounds.getBottom(), centralCoreIntensity);

        drawPulseNodes(g, cx, bounds.getBottom());

        // 3. СЕТЕВАЯ ИНТЕРФЕРЕНЦИЯ (Глитчи)
        if (state.netModulation > 0.05f) {
            drawNetworkArcs(g, cx, bounds.getBottom());
        } else if (random.nextFloat() < 0.1f) { // Случайные искры даже без сигнала
            drawNetworkArcs(g, cx, bounds.getBottom());
        }

        // 4. GLOBAL HEAT OVERLOAD (Белая вспышка)
        if (state.globalHeat > 0.01f) {
            drawOverloadFlash(g, bounds);
        }

        g.restoreState();

        // 5. ОПРАВА (RIM) - Fallout Style
        drawMetalRims(g, bounds);
    }

private:
    // --- RENDERERS ---

    void drawGlassContainer(juce::Graphics& g, juce::Rectangle<float> r)
    {
        // Фон - темная жидкость
        g.setColour(juce::Colour::fromFloatRGBA(0.05f, 0.05f, 0.07f, 0.6f));
        g.fillRoundedRectangle(r, 10.0f);

        // Внутреннее свечение стекла
        juce::ColourGradient glassGrad(
            juce::Colours::transparentBlack, r.getCentreX(), r.getY(),
            juce::Colours::white.withAlpha(0.05f), r.getX(), r.getY(), true);
        g.setGradientFill(glassGrad);
        g.fillRoundedRectangle(r, 10.0f);

        // Блики на стекле (трубка)
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.fillRect(r.getX() + 5, r.getY() + 10, 2.0f, r.getHeight() - 20);
        g.fillRect(r.getRight() - 7, r.getY() + 10, 1.0f, r.getHeight() - 20);
    }

    void drawPlasmaStrand(juce::Graphics& g, float cx, float h, float signal, juce::Colour baseCol, float side)
    {
        if (signal < 0.01f) return;

        float visibility = juce::jlimit(0.2f, 1.0f, signal);

        juce::Path p;
        p.startNewSubPath(cx, 0);

        // Параметры формы
        float amplitude = w_cache * (0.08f + visibility * 0.12f + state.driveLevel * 0.05f);
        float freq = 0.045f + state.driveLevel * 0.08f; // Драйв повышает частоту волны
        float harmonic = 0.12f + state.driveLevel * 0.05f;

        // Magnetic Bulge (раздутие в центре от громкости)
        // Добавляем низкочастотное "пузо"

        for (float y = 0; y <= h; y += 5.0f)
        {
            // Основная волна
            float wave = std::sin(y * freq - time * 2.0f * side);
            wave += 0.35f * std::sin(y * harmonic + time * 3.0f + side);

            // Добавляем турбулентность (Drive)
            float turbulence = std::sin(y * 0.2f + time * 5.0f) * state.driveLevel * 0.5f;

            // Window (сужение к краям)
            float window = std::sin((y / h) * juce::MathConstants<float>::pi);

            // Magnetic pressure (раздутие)
            float bulge = side * window * (visibility * 16.0f);

            float x = cx + (wave + turbulence) * amplitude * window + bulge;

            // Добавляем джиттер от сети
            x += jitter * window * 0.5f;

            p.lineTo(x, y);
        }

        // Добавляем мягкий glow
        float glowAlpha = juce::jlimit(0.2f, 1.0f, 0.35f + signal * 0.5f + state.driveLevel * 0.3f);
        g.setColour(baseCol.withAlpha(glowAlpha));
        g.strokePath(p, juce::PathStrokeType(5.0f + signal * 4.0f, juce::PathStrokeType::curved)); // Широкий ореол

        // Отрисовка Core
        g.setColour(baseCol.brighter(0.8f).withAlpha(0.95f));
        g.strokePath(p, juce::PathStrokeType(2.0f + visibility * 0.5f, juce::PathStrokeType::curved));
    }

    void drawCoreStrand(juce::Graphics& g, float cx, float h, float signal)
    {
        juce::Path p;
        p.startNewSubPath(cx, 0);

        // Центральная жила - более прямая, но "напряженная"
        for (float y = 0; y <= h; y += 5.0f)
        {
            // Высокочастотная вибрация (Electric hum)
            float hum = (random.nextFloat() - 0.5f) * 2.0f * state.driveLevel;
            p.lineTo(cx + hum, y);
        }

        // Цвет зависит от "нагрева" (Drive)
        // Low Drive -> Blue/White
        // High Drive -> White/Gold
        juce::Colour coreColor = juce::Colours::white;
        if (state.driveLevel > 0.5f) {
            coreColor = juce::Colour::fromFloatRGBA(1.0f, 0.9f, 0.6f, 1.0f); // Gold hot
        }

        // Intense Glow
        g.setColour(coreColor.withAlpha(signal * 0.6f));
        g.strokePath(p, juce::PathStrokeType(2.0f + state.driveLevel * 4.0f));

        // Pure White Center
        g.setColour(juce::Colours::white);
        g.strokePath(p, juce::PathStrokeType(1.0f));
    }

    void drawNetworkArcs(juce::Graphics& g, float cx, float h)
    {
        // Рисуем случайные электрические разряды поперек плазмы
        g.setColour(CoheraUI::kCyanNeon.withAlpha(0.7f));

        int numArcs = (int)(state.netModulation * 5.0f);
        for(int i=0; i<numArcs; ++i) {
            float y = random.nextFloat() * h;
            float arcWidth = 10.0f + state.netModulation * 30.0f;

            g.drawLine(cx - arcWidth, y, cx + arcWidth, y + (random.nextFloat()-0.5f)*10.0f, 1.0f);
        }
    }

    void drawOverloadFlash(juce::Graphics& g, juce::Rectangle<float> r)
    {
        // Заполняем сосуд белым светом ("Молоко")
        float alpha = state.globalHeat * state.globalHeat; // Гамма-коррекция для резкости
        g.setColour(juce::Colours::white.withAlpha(alpha * 0.4f));
        g.fillRoundedRectangle(r, 10.0f);
    }

    void drawPulseNodes(juce::Graphics& g, float cx, float h)
    {
        if (h <= 0.0f)
            return;

        int nodeCount = 3;
        float energy = juce::jlimit(0.2f, 1.0f, (state.leftSignal + state.rightSignal) * 0.5f);

        for (int i = 0; i < nodeCount; ++i)
        {
            float y = h * (i + 1) / (nodeCount + 1);
            float pulse = std::sin(time * 3.0f + i * 2.0f) * 0.5f + 0.5f;
            float nodeSize = (4.0f + pulse * 6.0f) * energy;

            juce::Colour pulseColour = CoheraUI::kCyanNeon.interpolatedWith(
                CoheraUI::kOrangeNeon, 0.5f + 0.5f * state.driveLevel);

            g.setColour(pulseColour.withAlpha((0.3f + pulse * 0.5f) * (0.6f + state.driveLevel * 0.4f)));
            g.fillEllipse(cx - nodeSize * 0.5f, y - nodeSize * 0.5f, nodeSize, nodeSize);
        }
    }

    void drawMetalRims(juce::Graphics& g, juce::Rectangle<float> r)
    {
        // Окантовка в стиле Fallout / Heavy Industry
        g.setColour(CoheraUI::kPanel.brighter(0.2f));

        // Верхнее кольцо
        g.drawRoundedRectangle(r.getX(), r.getY(), r.getWidth(), 15.0f, 4.0f, 2.0f);

        // Нижнее кольцо
        g.drawRoundedRectangle(r.getX(), r.getBottom() - 15.0f, r.getWidth(), 15.0f, 4.0f, 2.0f);

        // Болтики (Декор)
        g.setColour(CoheraUI::kTextDim.withAlpha(0.5f));
        float boltSize = 3.0f;
        g.fillEllipse(r.getX() + 5, r.getY() + 6, boltSize, boltSize);
        g.fillEllipse(r.getRight() - 8, r.getY() + 6, boltSize, boltSize);
        g.fillEllipse(r.getX() + 5, r.getBottom() - 9, boltSize, boltSize);
        g.fillEllipse(r.getRight() - 8, r.getBottom() - 9, boltSize, boltSize);
    }

    PlasmaState state;
    float time = 0.0f;
    float jitter = 0.0f;
    float w_cache = 0.0f; // Cache width
    juce::Random random;
};
