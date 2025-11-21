#pragma once

#include "../../JuceHeader.h"
#include "AbstractVisualizer.h"
#include "../CoheraLookAndFeel.h"
#include "../../dsp/MathSaturator.h"

// ==============================================================================
// ВНУТРЕННЯЯ СТРУКТУРА ДАННЫХ
// ==============================================================================

struct NebulaPoint {
    float input;  // X (-1..1)
    float output; // Y (-1..1)
    float age;    // 0.0 (New) .. 1.0 (Old)
    float distortion; // Разница между in/out (для цвета)
};

// ==============================================================================
// NEBULA SHAPER VISUALIZER
// ==============================================================================

class NebulaShaper : public AbstractVisualizer
{
public:
    NebulaShaper(CoheraSaturatorAudioProcessor& p)
        : AbstractVisualizer(20), // Уменьшили до 20 FPS для производительности
          processor(p),
          apvts(p.getAPVTS())
    {
        // Игнорируем мышь, чтобы можно было кликать сквозь (если оверлей)
        // Но если мы хотим переключать режимы по клику - убери эту строку.
        setInterceptsMouseClicks(false, false);

        // Инициализируем буфер истории
        history.resize(MAX_POINTS);
    }

protected:
    // --- PHYSICS UPDATE LOOP ---
    void updatePhysics() override
    {
        // 1. Читаем новые данные из FIFO процессора
        // Мы забираем столько точек, сколько успело накопиться
        float in, out;
        int pointsRead = 0;

        // Ограничиваем чтение за кадр, чтобы UI не вис при высоком SR
        while (pointsRead < 200 && processor.popVisualizerData(in, out))
        {
            addPoint(in, out);
            pointsRead++;
        }

        // 2. Обновляем параметры для кривой
        currentDrive = *apvts.getRawParameterValue("drive_master");
        float modeF = *apvts.getRawParameterValue("math_mode");
        currentMode = static_cast<Cohera::SaturationMode>((int)modeF);

        // 3. Эффект "Дыхания" кривой от RMS
        float rms = processor.getOutputRMS();
        curveJitter = curveJitter * 0.9f + (rms * 0.05f);
    }

    // --- RENDER LOOP ---
    void paint(juce::Graphics& g) override
    {
        if (getWidth() <= 0 || getHeight() <= 0) return;

        // A. Прозрачный режим - рисуем напрямую без frameBuffer
        // Для overlay на анализаторе нужен прозрачный фон

        // B. Рисуем облако точек напрямую (без trail для overlay)
        drawNebulaPoints(g);

        // C. Рисуем минимальную сетку для ориентации
        drawMinimalGrid(g);

        // D. Рисуем кривую передачи поверх
        drawTransferCurve(g);
    }

private:
    // --- HELPER METHODS (Private Implementation) ---

    void addPoint(float in, float out)
    {
        // Циклический буфер
        writePos = (writePos + 1) % MAX_POINTS;

        NebulaPoint& p = history[writePos];
        p.input = in;
        p.output = out;
        p.age = 0.0f; // Свежая точка

        // Вычисляем "температуру" точки (насколько сильно искажен сигнал)
        // Линейный сигнал: out == in. Сатурация: out != in.
        // Усиливаем разницу для визуализации
        p.distortion = std::abs(out - in * (1.0f + currentDrive / 10.0f));
    }

    void drawNebulaPoints(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();
        float scaleX = bounds.getWidth() * 0.45f;
        float scaleY = bounds.getHeight() * 0.45f;

        // Рисуем только последние N точек для производительности
        // Но проходим по всему буферу для затухания? Нет, только активные.
        // Для эффекта "Scatter Plot" рисуем случайное подмножество или все свежие.

        // Простая стратегия: рисуем последние 300 точек
        int count = 0;
        int idx = writePos;

        while (count < 300) {
            const auto& p = history[idx];

            // Координаты (Input -> X, Output -> Y)
            float x = cx + p.input * scaleX;
            float y = cy - p.output * scaleY; // Y перевернут в UI

            // Цвет зависит от искажения
            // Малое искажение -> Cyan (Cold)
            // Сильное искажение -> Orange/White (Hot)

            juce::Colour color;
            float size;

            if (p.distortion > 0.1f) {
                // Hot Particle - ярко-красный/оранжевый
                color = juce::Colours::orangered;
                size = 4.0f + p.distortion * 8.0f; // Вздувается при клиппинге
            } else {
                // Cold Particle - ярко-голубой
                color = juce::Colours::deepskyblue;
                size = 3.0f;
            }

            // Альфа зависит от "возраста" в цикле отрисовки (чем дальше от writePos, тем прозрачнее)
            float alpha = 1.0f - ((float)count / 300.0f);

            g.setColour(color.withAlpha(alpha * 0.8f));
            g.fillEllipse(x - size/2, y - size/2, size, size);

            // Move backwards
            idx--;
            if (idx < 0) idx = MAX_POINTS - 1;
            count++;
        }
    }

    void drawTransferCurve(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();
        float cx = bounds.getCentreX();
        float cy = bounds.getCentreY();

        juce::Path curve;
        bool first = true;

        // Эффективный драйв для визуализации формы кривой
        float vizDrive = 1.0f + (currentDrive / 20.0f);

        // Строим идеальную мат. модель
        for (float x = -1.0f; x <= 1.0f; x += 0.05f)
        {
            float y = saturator.processSample(x, vizDrive, currentMode);

            // Нормализация для экрана
            y = juce::jlimit(-1.1f, 1.1f, y);

            float sx = cx + x * bounds.getWidth() * 0.45f;
            float sy = cy - y * bounds.getHeight() * 0.45f;

            // Добавляем дрожание от энергии (Bass Shake)
            if (std::abs(y) > 0.5f) { // Трясутся только края (клиппинг)
                 sy += (random.nextFloat() - 0.5f) * curveJitter * 40.0f;
            }

            if (first) { curve.startNewSubPath(sx, sy); first = false; }
            else { curve.lineTo(sx, sy); }
        }

        // Яркая неоновая обводка для контраста
        g.setColour(juce::Colours::cyan.withAlpha(0.8f)); // Bright cyan glow
        g.strokePath(curve, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved));

        g.setColour(juce::Colours::white); // Bright core
        g.strokePath(curve, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved));
    }

    void drawMinimalGrid(juce::Graphics& g)
    {
        auto bounds = getLocalBounds().toFloat();

        // Простая сетка координат для ориентации
        g.setColour(juce::Colours::white.withAlpha(0.3f));
        g.drawLine(bounds.getCentreX(), 0, bounds.getCentreX(), bounds.getHeight(), 1.0f);
        g.drawLine(0, bounds.getCentreY(), bounds.getWidth(), bounds.getCentreY(), 1.0f);
    }

    // Data
    CoheraSaturatorAudioProcessor& processor;
    juce::AudioProcessorValueTreeState& apvts;

    // History Buffer
    static constexpr int MAX_POINTS = 1000;
    std::vector<NebulaPoint> history;
    int writePos = 0;

    // Visual State
    MathSaturator saturator;
    juce::Random random;

    // Params Cache
    float currentDrive = 0.0f;
    Cohera::SaturationMode currentMode = Cohera::SaturationMode::GoldenRatio;
    float curveJitter = 0.0f;
};
