#pragma once

#include <cmath>
#include <algorithm>
#include "../JuceHeader.h"

class TransientSplitter
{
public:
    void prepare(double sampleRate)
    {
        // Следим за телом сигнала.
        // Атака: 20мс (пропускаем быстрые пики)
        // Релиз: 200мс (держим сустейн)
        slewAttack = std::exp(-1.0f / (0.02f * (float)sampleRate));
        slewRelease = std::exp(-1.0f / (0.20f * (float)sampleRate));

        envelope = 0.0f;
    }

    void reset() {
        envelope = 0.0f;
    }

    struct SplitResult {
        float trans;
        float body;
    };

    // Разделяет входящий сэмпл на две составляющие
    SplitResult process(float input)
    {
        float absIn = std::abs(input);

        // 1. Следим за "Телом" (Slow Envelope)
        if (absIn > envelope)
            envelope = envelope * slewAttack + absIn * (1.0f - slewAttack);
        else
            envelope = envelope * slewRelease + absIn * (1.0f - slewRelease);

        // 2. Вычисляем соотношение (Ratio)
        // Если сигнал (absIn) сильно больше огибающей тела (envelope) -> это Транзиент.
        // transRatio = (Input - BodyEnv) / Input

        float transRatio = 0.0f;

        // Защита от деления на ноль
        if (absIn > 0.00001f)
        {
            // Разница между мгновенным пиком и телом
            float diff = std::max(0.0f, absIn - envelope);
            // Доля транзиента в сигнале
            transRatio = diff / absIn;
        }

        // Усиливаем разделение (Curve), чтобы четче резать
        transRatio = std::pow(transRatio, 1.5f);
        transRatio = juce::jlimit(0.0f, 1.0f, transRatio);

        // 3. Разделяем аудио (Кроссфейд)
        // Важно: trans + body = input. Фаза сохраняется.
        float transSample = input * transRatio;
        float bodySample = input * (1.0f - transRatio);

        return { transSample, bodySample };
    }

private:
    float envelope = 0.0f;
    float slewAttack = 0.0f;
    float slewRelease = 0.0f;
};
