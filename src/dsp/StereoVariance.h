#pragma once

#include <cmath>

class StereoVariance
{
public:
    void prepare(double sampleRate)
    {
        fs = sampleRate;
        // LFO 1 (Left): Очень медленный (0.1 Hz)
        phaseIncL = (2.0f * juce::MathConstants<float>::pi * 0.1f) / (float)fs;
        // LFO 2 (Right): Чуть быстрее и не кратный (0.143 Hz) - чтобы не было синхрона
        phaseIncR = (2.0f * juce::MathConstants<float>::pi * 0.143f) / (float)fs;

        phaseL = 0.0f;
        phaseR = 2.0f; // Стартуем со сдвигом фазы
    }

    struct DriftValues {
        float driveMultL;
        float driveMultR;
    };

    // amount: 0.0 ... 1.0 (глубина расстройки)
    DriftValues getDrift(float amount)
    {
        // Обновляем фазы LFO
        phaseL += phaseIncL;
        if (phaseL > juce::MathConstants<float>::twoPi) phaseL -= juce::MathConstants<float>::twoPi;

        phaseR += phaseIncR;
        if (phaseR > juce::MathConstants<float>::twoPi) phaseR -= juce::MathConstants<float>::twoPi;

        // Генерируем дрейф +/- 5% при максимальном amount
        // Используем sin для плавности
        float rawL = std::sin(phaseL);
        float rawR = std::sin(phaseR);

        // Масштабируем:
        // Если amount = 1.0, множитель плавает от 0.95 до 1.05
        float scale = 0.05f * amount;

        return { 1.0f + rawL * scale, 1.0f + rawR * scale };
    }

    // Crosstalk (Взаимопроникновение каналов)
    // Подмешиваем чуть-чуть L в R и наоборот.
    // Это "склеивает" стереокартину.
    void applyCrosstalk(float& l, float& r, float amount)
    {
        if (amount < 0.01f) return;

        // Эмулируем утечку тока: -60dB .. -40dB
        float bleed = amount * 0.01f; // макс 1% утечки

        float oldL = l;
        float oldR = r;

        l = oldL * (1.0f - bleed) + oldR * bleed;
        r = oldR * (1.0f - bleed) + oldL * bleed;
    }

private:
    double fs = 44100.0;
    float phaseL = 0.0f;
    float phaseR = 0.0f;
    float phaseIncL = 0.0f;
    float phaseIncR = 0.0f;
};
