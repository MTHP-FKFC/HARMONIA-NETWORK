#pragma once
#include "../JuceHeader.h"

// Размер FFT (чем больше, тем точнее низ, но медленнее)
static constexpr int fftOrder = 11;
static constexpr int fftSize  = 1 << fftOrder; // 2048
static constexpr int scopeSize = 512; // Разрешение для рисования

class SimpleFFT
{
public:
    SimpleFFT() : forwardFFT(fftOrder), window(fftSize, juce::dsp::WindowingFunction<float>::hann)
    {
    }

    void prepare()
    {
        // Инициализация
        std::fill(scopeData.begin(), scopeData.end(), 0.0f);
    }

    // Вызывается из processBlock (Audio Thread) - должно быть супер-быстро!
    void pushSample(float sample)
    {
        // Если FIFO полон, старые данные просто сгорают (это норм для визуализации)
        if (fifoIndex.load() < fftSize)
        {
            fifo[fifoIndex.load()] = sample;
            fifoIndex++;
        }
        else
        {
            // Буфер полон, помечаем, что готовы считать
            // В идеале тут нужен RingBuffer, но для простоты делаем "One-Shot" буфер
            // Если UI не успел прочитать - ждем следующего цикла.
            if (!nextBlockReady.load())
            {
                // Копируем во временный буфер для обработки
                std::copy(fifo.begin(), fifo.end(), fftData.begin());
                nextBlockReady.store(true);
            }
            fifoIndex.store(0);
        }
    }

    // Вызывается из TimerCallback (GUI Thread)
    void process(float decay = 0.85f)
    {
        if (nextBlockReady.compare_exchange_strong(isReady, false)) // Если есть новые данные
        {
            // 1. Окно
            window.multiplyWithWindowingTable(fftData.data(), fftSize);

            // 2. FFT
            forwardFFT.performFrequencyOnlyForwardTransform(fftData.data());

            // 3. Маппинг в Scope (логарифмический уровень) и сглаживание
            // fftData имеет размер fftSize/2 (спектр)
            // Нам нужно отобразить это в scopeSize точек
            
            for (int i = 0; i < scopeSize; ++i)
            {
                // Берем диапазон частот
                // Для красоты можно сделать логарифмический маппинг индекса, но пока линейно
                int fftIdx = (int)juce::jmap((float)i, 0.0f, (float)scopeSize, 0.0f, (float)(fftSize / 2));
                
                float level = fftData[(size_t)fftIdx];
                
                // Нормализация и перевод в 0..1 для высоты графика
                // level обычно может быть большим, берем Decibels
                float levelDb = juce::Decibels::gainToDecibels(level) - juce::Decibels::gainToDecibels((float)fftSize);
                
                // Мапим dB (-100 .. 0) в (0 .. 1)
                float scaled = juce::jmap(levelDb, -100.0f, 0.0f, 0.0f, 1.0f);
                scaled = juce::jlimit(0.0f, 1.0f, scaled);

                // Атака мгновенная, Релиз плавный
                if (scaled > scopeData[i])
                    scopeData[i] = scaled;
                else
                    scopeData[i] = scopeData[i] * decay + scaled * (1.0f - decay);
            }
        }
        else
        {
            // Если новых данных нет, просто затухаем старые
            for (auto& v : scopeData) v *= decay;
        }
    }

    const std::array<float, scopeSize>& getScopeData() const { return scopeData; }

private:
    juce::dsp::FFT forwardFFT;
    juce::dsp::WindowingFunction<float> window;

    std::array<float, fftSize> fifo;
    std::array<float, fftSize * 2> fftData; // *2 нужно для performFrequencyOnlyForwardTransform
    
    std::atomic<int> fifoIndex { 0 };
    std::atomic<bool> nextBlockReady { false };
    bool isReady = true; // dummy helper for compare_exchange

    std::array<float, scopeSize> scopeData; // Данные для отрисовки
};