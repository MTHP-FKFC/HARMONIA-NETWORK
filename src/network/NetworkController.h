#pragma once

#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"
#include "INetworkManager.h"
#include "../dsp/Envelope.h"
#include <juce_audio_processors/juce_audio_processors.h>

namespace Cohera {

/**
 * @brief Network Controller - Handles inter-instance communication
 * REAL-TIME SAFE VERSION:
 * - No logging in process()
 * - Correct smoothing applied
 * - Optimized atomic reads
 * 
 * Clean Architecture pattern:
 * - Business logic for network modulation (Reference/Listener roles)
 * - Delegates storage to INetworkManager (Dependency Injection)
 * - No direct dependency on Singleton
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
        inputFollower.reset(sampleRate);
        
        // Настраиваем сглаживатели входящего сигнала
        // Время атаки/релиза сглаживания должно быть коротким, чтобы не "съедать" транзиенты,
        // но достаточным, чтобы убрать ступенчатость (zipper noise) от обновления раз в блок.
        for(auto& smooth : receivedEnvelopes) {
            smooth.reset(sampleRate, 0.005); // 5ms smoothing (быстро, но плавно)
        }
    }

    void reset()
    {
        inputFollower.reset(44100.0); // SR обновится при prepare
        for(auto& smooth : receivedEnvelopes) smooth.setCurrentAndTargetValue(0.0f);
    }

    /**
     * @brief Process network communication
     * @return Array of control values (0.0 - 1.0) ready for modulation
     */
    std::array<float, kNumBands> process(const juce::AudioBuffer<float>& inputBuffer, 
                                         const ParameterSet& params)
    {
        std::array<float, kNumBands> modulations;
        modulations.fill(0.0f); // По умолчанию модуляции нет

        // --- 1. ЛОГИКА ОТПРАВИТЕЛЯ (REFERENCE) ---
        // Если мы управляем другими (например, мы - Бочка)
        if (params.netRole == NetworkRole::Reference)
        {
            // Оптимизация: Берем пик блока, а не RMS, так быстрее для дакинга
            // getMagnitude - очень быстрая векторная операция в JUCE
            float magnitude = inputBuffer.getMagnitude(0, inputBuffer.getNumSamples());
            
            // Прогоняем через Envelope Follower (Атака/Релиз)
            float envelope = inputFollower.process(magnitude);

            // Отправляем данные в "Облако" (Shared Memory)
            // Пока отправляем широковещательно во все полосы (Broadband trigger)
            if (envelope > 0.001f) // Не спамим нулями в сеть
            {
                for (int i = 0; i < kNumBands; ++i) {
                    networkManager.updateBandSignal(params.groupId, i, envelope);
                }
            }
        }

        // --- 2. ЛОГИКА ПОЛУЧАТЕЛЯ (LISTENER) ---
        // Если мы слушаем других (например, мы - Бас)
        if (params.netRole == NetworkRole::Listener)
        {
            for (int i = 0; i < kNumBands; ++i)
            {
                // 1. Читаем атомик (Thread-safe lock-free read)
                float rawNet = networkManager.getBandSignal(params.groupId, i);

                // 2. Сглаживание (Anti-Zipper Noise)
                // Важно: данные из сети приходят раз в блок (дискретно).
                // Нам нужно превратить ступеньки в плавную линию.
                receivedEnvelopes[i].setTargetValue(rawNet);
                float smoothed = receivedEnvelopes[i].getNextValue();

                // 3. Чувствительность (Sensitivity)
                // Sens = 0 -> нет реакции, Sens = 1 -> полная, Sens > 1 -> овердрайв реакции
                float processed = smoothed * params.netSens;

                // Клэмп, чтобы математика дальше не сломалась
                modulations[i] = std::fmin(1.0f, processed);
            }
        }

        return modulations;
    }

private:
    INetworkManager& networkManager;
    
    // Детектор для анализа своего сигнала
    EnvelopeFollower inputFollower;
    
    // Сглаживатели для чужого сигнала (по одному на полосу)
    std::array<juce::LinearSmoothedValue<float>, kNumBands> receivedEnvelopes;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(NetworkController)
};

} // namespace Cohera