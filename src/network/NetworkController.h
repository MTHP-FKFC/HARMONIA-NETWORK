#pragma once

#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
#include "NetworkManager.h"
#include "../dsp/Envelope.h" // Используем наш существующий класс

namespace Cohera {

class NetworkController
{
public:
    void prepare(double sampleRate)
    {
        // Envelope follower для анализа входящего сигнала (если мы Reference)
        inputFollower.reset(sampleRate);

        // Сглаживатели для приема данных (чтобы модуляция была плавной)
        for(auto& smooth : receivedEnvelopes) {
            smooth.reset(sampleRate, 0.02); // 20ms smoothing
        }
    }

    void reset()
    {
        inputFollower.reset(44100.0); // SR обновится при prepare
        for(auto& smooth : receivedEnvelopes) smooth.setCurrentAndTargetValue(0.0f);
    }

    // Возвращает массив модуляций для 6 полос (0.0 .. 1.0)
    std::array<float, kNumBands> process(const juce::AudioBuffer<float>& inputBuffer, const ParameterSet& params)
    {
        std::array<float, kNumBands> modulations;
        modulations.fill(0.0f);

        // 1. Логика Reference (Отправитель)
        if (params.netRole == NetworkRole::Reference)
        {
            // Измеряем пик входного сигнала (Max Magnitude)
            float magnitude = inputBuffer.getMagnitude(0, inputBuffer.getNumSamples());
            float envelope = inputFollower.process(magnitude);

            // Отправляем в сеть (во все полосы одинаковый сигнал для простоты v1.3)
            // В будущем можно сделать мультибанд-анализ
            for (int i = 0; i < kNumBands; ++i) {
                NetworkManager::getInstance().updateBandSignal(params.groupId, i, envelope);
            }
        }

        // 2. Логика Listener (Получатель)
        // Мы читаем сеть, даже если мы Reference (чтобы видеть свой сигнал? нет, обычно нет).
        // Но если мы Listener - это критично.

        if (params.netRole == NetworkRole::Listener)
        {
            for (int i = 0; i < kNumBands; ++i)
            {
                // Читаем сырое значение из атомика
                float rawNet = NetworkManager::getInstance().getBandSignal(params.groupId, i);

                // Сглаживаем
                receivedEnvelopes[i].setTargetValue(rawNet);
                float smoothed = receivedEnvelopes[i].getNextValue();

                // Применяем настройки (Depth, Sensitivity)
                // Sens > 1.0 усиливает реакцию
                float processed = std::min(1.0f, smoothed * params.netSens);

                // Depth миксует между 0 и сигналом
                modulations[i] = processed * params.netDepth;
            }
        }

        // 3. Global Heat Logic (Отправляем свою энергию)
        // RMS за блок
        float rms = inputBuffer.getRMSLevel(0, 0, inputBuffer.getNumSamples());
        // TODO: Нужно где-то хранить свой ID instance. Пока опустим, добавим позже.

        return modulations;
    }

private:
    EnvelopeFollower inputFollower;
    std::array<juce::LinearSmoothedValue<float>, kNumBands> receivedEnvelopes;
};

} // namespace Cohera
