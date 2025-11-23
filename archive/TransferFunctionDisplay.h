#pragma once
#include "AbstractVisualizer.h"
#include "../CoheraLookAndFeel.h"
#include "../../dsp/MathSaturator.h"
#include "../../CoheraTypes.h"

class TransferFunctionDisplay : public AbstractVisualizer
{
public:
    TransferFunctionDisplay() : AbstractVisualizer(30) // 30 FPS достаточно
    {
        setInterceptsMouseClicks(false, false);
    }

    // Установка параметров для кривой
    void setParameters(float drive, Cohera::SaturationMode mode, float inputLevel)
    {
        this->drive = drive;
        this->mode = mode;
        this->inputLevel = juce::jlimit(-1.0f, 1.0f, inputLevel);
    }

    // Установка cascade режима
    void setCascadeMode(bool cascade)
    {
        this->cascade = cascade;
    }

protected:
    void updatePhysics() override
    {
        // Плавное движение точки сигнала
        smoothedInput = smoothedInput * 0.8f + inputLevel * 0.2f;

        // Добавляем небольшое дрожание для живости
        jitter = (random.nextFloat() - 0.5f) * 0.02f * (1.0f + drive * 0.1f);
    }

    void paint(juce::Graphics& g) override
    {
        if (!isVisible() || getWidth() == 0 || getHeight() == 0) return;

        auto bounds = getLocalBounds().toFloat().reduced(2);
        auto center = bounds.getCentre();

        // 1. Фон (полупрозрачное стекло с градиентом для overlay)
        juce::ColourGradient bgGrad(
            juce::Colour(15, 15, 20).withAlpha(0.3f), bounds.getTopLeft(),
            juce::Colour(25, 25, 35).withAlpha(0.2f), bounds.getBottomRight(), false);
        g.setGradientFill(bgGrad);
        g.fillRoundedRectangle(bounds, 4.0f);

        // Рамка
        g.setColour(CoheraUI::kTextDim.withAlpha(0.3f));
        g.drawRoundedRectangle(bounds, 4.0f, 1.0f);

        // 2. Оси координат
        g.setColour(CoheraUI::kTextDim.withAlpha(0.2f));
        g.drawLine(bounds.getX(), center.y, bounds.getRight(), center.y, 1.0f); // X ось
        g.drawLine(center.x, bounds.getY(), center.x, bounds.getBottom(), 1.0f); // Y ось

        // Метки осей
        g.setColour(CoheraUI::kTextDim.withAlpha(0.4f));
        g.setFont(8.0f);
        g.drawText("-1", bounds.getX() + 2, center.y - 12, 20, 10, juce::Justification::left);
        g.drawText("1", bounds.getRight() - 22, center.y - 12, 20, 10, juce::Justification::right);
        g.drawText("1", center.x - 10, bounds.getY() + 2, 20, 10, juce::Justification::centred);
        g.drawText("-1", center.x - 10, bounds.getBottom() - 12, 20, 10, juce::Justification::centred);

        // 3. Кривая передачи (Transfer Curve)
        drawTransferCurve(g, bounds, center);

        // 4. Точка текущего сигнала
        drawSignalPoint(g, bounds, center);

        // 5. Trail эффект (история сигнала)
        drawSignalTrail(g, bounds, center);
    }

private:
    void drawTransferCurve(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Point<float> center)
    {
        juce::Path curve;
        bool start = true;

        // Эффективный драйв для визуализации
        float vizDrive = 1.0f + (drive / 20.0f);

        // Создаем точки кривой
        std::vector<juce::Point<float>> curvePoints;

        for (float x = -1.0f; x <= 1.01f; x += 0.02f)
        {
        // Use real MathSaturator for accurate transfer function
        float y = mathSaturator.processSample(x, vizDrive, mode);
            y = juce::jlimit(-1.1f, 1.1f, y);

            float sx = center.x + (x * bounds.getWidth() * 0.4f);
            float sy = center.y - (y * bounds.getHeight() * 0.4f);

            if (start) {
                curve.startNewSubPath(sx, sy);
                start = false;
            } else {
                curve.lineTo(sx, sy);
            }
        }

        // Glow эффект для кривой
        g.setColour(CoheraUI::kOrangeNeon.withAlpha(0.2f));
        g.strokePath(curve, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved));

        // Основная кривая
        g.setColour(CoheraUI::kOrangeNeon);
        g.strokePath(curve, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved));
    }

    void drawSignalPoint(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Point<float> center)
    {
        // Позиция точки по входному сигналу
        float dotX = center.x + (smoothedInput * bounds.getWidth() * 0.4f);

        // Выходной сигнал через сатуратор
        float vizDrive = 1.0f + (drive / 20.0f);
        float outputY = mathSaturator.processSample(smoothedInput, vizDrive, mode);
        outputY = juce::jlimit(-1.1f, 1.1f, outputY);

        float dotY = center.y - (outputY * bounds.getHeight() * 0.4f);

        // Добавляем jitter
        dotX += jitter * bounds.getWidth() * 0.1f;
        dotY += jitter * bounds.getHeight() * 0.1f;

        // Свечение точки
        g.setColour(CoheraUI::kCyanNeon.withAlpha(0.3f));
        g.fillEllipse(dotX - 6, dotY - 6, 12, 12);

        // Ядро точки
        g.setColour(CoheraUI::kCyanNeon);
        g.fillEllipse(dotX - 3, dotY - 3, 6, 6);

        // Ободок
        g.setColour(juce::Colours::white.withAlpha(0.8f));
        g.drawEllipse(dotX - 3, dotY - 3, 6, 6, 1.5f);
    }

    void drawSignalTrail(juce::Graphics& g, juce::Rectangle<float> bounds, juce::Point<float> center)
    {
        // Сохраняем историю точек
        juce::Point<float> currentPoint(
            center.x + (smoothedInput * bounds.getWidth() * 0.4f),
            center.y - (std::tanh(smoothedInput * (1.0f + (drive / 20.0f))) * bounds.getHeight() * 0.4f) // Simple fallback
        );

        trailPoints.push_back(currentPoint);

        // Ограничиваем длину trail
        if (trailPoints.size() > 15) {
            trailPoints.erase(trailPoints.begin());
        }

        // Рисуем trail
        if (trailPoints.size() > 1) {
            juce::Path trail;
            trail.startNewSubPath(trailPoints[0]);

            for (size_t i = 1; i < trailPoints.size(); ++i) {
                trail.lineTo(trailPoints[i]);
            }

            // Fade out эффект
            for (int i = trailPoints.size() - 1; i >= 0; --i) {
                float alpha = (float)i / trailPoints.size() * 0.4f;
                g.setColour(CoheraUI::kCyanNeon.withAlpha(alpha));
                g.fillEllipse(trailPoints[i].x - 1, trailPoints[i].y - 1, 2, 2);
            }
        }
    }

    MathSaturator mathSaturator;
    juce::Random random;

    float drive = 0.0f;
    Cohera::SaturationMode mode = Cohera::SaturationMode::GoldenRatio;
    float inputLevel = 0.0f;
    float smoothedInput = 0.0f;
    float jitter = 0.0f;
    bool cascade = false;

    std::vector<juce::Point<float>> trailPoints;
};
