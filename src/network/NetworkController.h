#pragma once

#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
#include "INetworkManager.h"
#include "../dsp/Envelope.h" // Используем наш существующий класс
#include <juce_audio_processors/juce_audio_processors.h>

namespace Cohera {

/**
 * @brief Network Controller - Handles inter-instance communication
 * 
 * Clean Architecture pattern:
 * - Business logic for network modulation (Reference/Listener roles)
 * - Delegates storage to INetworkManager (Dependency Injection)
 * - No direct dependency on Singleton
 * 
 * Responsibilities:
 * 1. Analyze input signal (if Reference)
 * 2. Send band envelopes to network (if Reference)
 * 3. Receive band envelopes from network (if Listener)
 * 4. Smooth and scale received modulations
 * 
 * Thread Safety:
 * - All operations are lock-free (uses atomic operations via INetworkManager)
 * - Safe to call from real-time audio thread
 */
class NetworkController
{
public:
    /**
     * @brief Constructor with Dependency Injection
     * @param manager Reference to network manager (can be real or mock)
     */
    explicit NetworkController(INetworkManager& manager)
        : networkManager(manager)
    {
    }

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

    /**
     * @brief Process network communication for current block
     * @param inputBuffer Input audio buffer (for analysis if Reference)
     * @param params Current parameter state (role, group, depth, etc.)
     * @return Array of 6 modulation values (0.0 .. 1.0) for each band
     */
    std::array<float, kNumBands> process(const juce::AudioBuffer<float>& inputBuffer, 
                                         const ParameterSet& params)
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
                networkManager.updateBandSignal(params.groupId, i, envelope);
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
                float rawNet = networkManager.getBandSignal(params.groupId, i);

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
    INetworkManager& networkManager; // Dependency Injection (interface, not Singleton!)
    
    EnvelopeFollower inputFollower;
    std::array<juce::LinearSmoothedValue<float>, kNumBands> receivedEnvelopes;
};

} // namespace Cohera