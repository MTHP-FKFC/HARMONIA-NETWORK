#pragma once

#include "AbstractVisualizer.h"

class AnalogNoise : public AbstractVisualizer
{
public:
    AnalogNoise() : AbstractVisualizer(20) // Низкий FPS для эффекта старого кино
    {
        setInterceptsMouseClicks(false, false);
    }

    void resized() override {
        // Генерируем текстуру шума при ресайзе (чуть больше экрана для сдвига)
        int w = getWidth() + 50;
        int h = getHeight() + 50;
        if (w <= 50 || h <= 50) return;

        noiseImg = juce::Image(juce::Image::SingleChannel, w, h, true);
        juce::Graphics g(noiseImg);

        juce::Random rng;
        for(int i=0; i < (w*h)/10; ++i) { // 10% плотность
            float x = rng.nextFloat() * w;
            float y = rng.nextFloat() * h;
            float brightness = rng.nextFloat() * 0.1f; // Очень тускло
            g.setColour(juce::Colours::white.withAlpha(brightness));
            g.fillRect(x, y, 1.5f, 1.5f);
        }
    }

protected:
    void updatePhysics() override
    {
        // Случайный сдвиг координат каждый кадр
        offsetX = random.nextInt(50) - 25;
        offsetY = random.nextInt(50) - 25;
    }

    void paint(juce::Graphics& g) override
    {
        if (noiseImg.isValid()) {
            g.setOpacity(0.4f + currentEnergy * 0.2f); // Шум усиливается от громкости
            g.drawImageAt(noiseImg, offsetX, offsetY);
        }
    }

private:
    juce::Image noiseImg;
    int offsetX = 0, offsetY = 0;
    juce::Random random;
};
