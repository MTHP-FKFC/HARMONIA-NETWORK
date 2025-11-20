#pragma once
#include "../../JuceHeader.h"
#include "../../ui/SimpleFFT.h"

class SpectrumVisor : public juce::Component, private juce::Timer
{
public:
    SpectrumVisor(SimpleFFT& fftToUse) : fft(fftToUse)
    {
        // 30 FPS достаточно для плавности
        startTimerHz(30);
    }

    ~SpectrumVisor() override { stopTimer(); }

    void timerCallback() override
    {
        // Запускаем математику FFT (это быстро)
        fft.process(0.85f); // Decay factor
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        auto w = area.getWidth();
        auto h = area.getHeight();

        // 1. Фон
        g.setColour(juce::Colour::fromRGB(15, 17, 20));
        g.fillRoundedRectangle(area, 4.0f);

        // 2. Сетка (Grid)
        g.setColour(juce::Colours::white.withAlpha(0.05f));
        // Рисуем линии на 100, 1000, 10000 Гц
        float freqPoints[] = { 100.0f, 1000.0f, 10000.0f };
        for (float freq : freqPoints)
        {
            float x = mapFreqToX(freq, w);
            g.drawVerticalLine((int)x, 0.0f, h);
            
            g.setFont(10.0f);
            g.drawText(juce::String((int)freq) + "Hz", (int)x + 2, (int)h - 12, 40, 10, juce::Justification::left);
        }

        // 3. Спектр
        const auto& data = fft.getScopeData();
        juce::Path p;
        p.startNewSubPath(0, h); // Старт в левом нижнем углу

        for (int i = 0; i < (int)data.size(); ++i)
        {
            // Логарифмическая ось X (для аудио это правильно)
            // data[i] уже линейно распределена по бинам FFT, 
            // но для графика нам нужно растянуть низы и сжать верха.
            // Хак для простоты: просто рисуем линейно по data, так как SimpleFFT::process 
            // делает примитивный маппинг. Для PRO нужен Log-Mapping в SimpleFFT.
            // Пока рисуем как есть:
            
            float x = juce::jmap((float)i, 0.0f, (float)data.size(), 0.0f, w);
            float y = juce::jmap(data[i], 0.0f, 1.0f, h, 0.0f); // 0 = низ экрана, 1 = верх
            
            // Сглаживаем путь (Curve)
            if (i == 0) p.lineTo(x, y);
            else        p.lineTo(x, y);
        }
        
        // Замыкаем путь вниз
        p.lineTo(w, h);
        p.closeSubPath();

        // Рисуем заливку (Gradient)
        // Сверху (громко) - Оранжевый, Снизу (тихо) - Прозрачный
        juce::ColourGradient grad(juce::Colour::fromRGB(255, 140, 0).withAlpha(0.6f), 0, 0,
                                  juce::Colour::fromRGB(255, 140, 0).withAlpha(0.0f), 0, h, false);
        g.setGradientFill(grad);
        g.fillPath(p);

        // Рисуем контур
        g.setColour(juce::Colour::fromRGB(255, 200, 100));
        g.strokePath(p, juce::PathStrokeType(1.5f));
    }
    
    // Хелпер для сетки (Log scale mapping)
    float mapFreqToX(float freq, float w)
    {
        // 20Hz ... 20000Hz
        return w * (std::log10(freq / 20.0f) / std::log10(20000.0f / 20.0f));
    }

private:
    SimpleFFT& fft;
};