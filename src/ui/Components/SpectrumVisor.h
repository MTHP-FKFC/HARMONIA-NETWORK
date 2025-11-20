#pragma once
#include "../../JuceHeader.h"
#include "../../ui/SimpleFFT.h"
#include "../../network/NetworkManager.h"

class SpectrumVisor : public juce::Component, private juce::Timer
{
public:
    SpectrumVisor(SimpleFFT& fftToUse, juce::AudioProcessorValueTreeState& apvts)

        : fft(fftToUse), apvts(apvts)
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

        // 4. GHOST SPECTRUM (Reference) - "Линия"
        // Рисуем только если мы Listener (чтобы видеть, под кого прогибаемся)
        bool isRef = *apvts.getRawParameterValue("role") > 0.5f;
        if (!isRef)
        {
            drawGhostCurve(g, w, h);
        }
    }
    
    // Хелпер для сетки (Log scale mapping)
    float mapFreqToX(float freq, float w)
    {
        // 20Hz ... 20000Hz
        return w * (std::log10(freq / 20.0f) / std::log10(20000.0f / 20.0f));
    }

    void drawGhostCurve(juce::Graphics& g, float w, float h)
    {
        int group = (int)*apvts.getRawParameterValue("group_id");
        auto& net = NetworkManager::getInstance();

        juce::Path ghostPath;
        bool first = true;

        // У нас 6 полос. Мы знаем их примерные центры (логарифмически).
        // 0: Sub (~60Hz), 1: Low (~150Hz), 2: Mid (~400Hz), 3: HM (~1.5k), 4: Hi (~4k), 5: Air (~10k)
        // Эти координаты (0.0-1.0 по X) подобраны эмпирически под Log шкалу
        float bandXPositions[6] = { 0.15f, 0.3f, 0.45f, 0.6f, 0.75f, 0.9f };

        for (int i = 0; i < 6; ++i)
        {
            // Получаем энергию полосы референса (0..1)
            float energy = net.getBandSignal(group, i);

            // Мапим на экран
            float x = w * bandXPositions[i];
            // Чуть усиливаем визуально, чтобы было видно
            float y = h - (energy * h * 0.9f);

            if (first) {
                ghostPath.startNewSubPath(0, h); // Старт слева внизу
                ghostPath.lineTo(x, y);
                first = false;
            } else {
                // Рисуем кривые (Rounded)
                ghostPath.lineTo(x, y);
            }
        }
        ghostPath.lineTo(w, h); // Финиш справа внизу

        // Рисуем "Призрака" (Мятный цвет - Network)
        g.setColour(juce::Colour::fromRGB(0, 255, 200).withAlpha(0.4f));
        // Пунктирная линия или просто толстая полупрозрачная
        g.strokePath(ghostPath, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved));

        // Заливка под призраком (очень слабая)
        g.setColour(juce::Colour::fromRGB(0, 255, 200).withAlpha(0.1f));
        ghostPath.closeSubPath();
        g.fillPath(ghostPath);
    }

private:
    SimpleFFT& fft;
    juce::AudioProcessorValueTreeState& apvts;
};