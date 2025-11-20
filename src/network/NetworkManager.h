#pragma once

#include <array>
#include <atomic>

// Максимальное кол-во инстансов (с запасом)
static constexpr int MAX_INSTANCES = 64;
// Кол-во групп (0..7)
static constexpr int MAX_GROUPS = 8;
static constexpr int NUM_BANDS = 6; // Наши 6 полос

class NetworkManager
{
public:
    // Синглтон (стандартный паттерн)
    static NetworkManager& getInstance()
    {
        static NetworkManager instance;
        return instance;
    }

    // === API для Аудио Потока (REALTIME SAFE) ===

    // 1. Reference пишет энергию конкретной полосы
    // groupIdx: 0-7, bandIdx: 0-5
    // value: 0.0 - 1.0 (Envelope полосы)
    void updateBandSignal(int groupIdx, int bandIdx, float value)
    {
        if (groupIdx >= 0 && groupIdx < MAX_GROUPS && bandIdx >= 0 && bandIdx < NUM_BANDS)
        {
            // store(memory_order_relaxed) - самая быстрая операция
            groupBandSignals[groupIdx][bandIdx].store(value, std::memory_order_relaxed);
        }
    }

    // 2. Listener читает энергию полосы
    float getBandSignal(int groupIdx, int bandIdx) const
    {
        if (groupIdx >= 0 && groupIdx < MAX_GROUPS && bandIdx >= 0 && bandIdx < NUM_BANDS)
        {
            return groupBandSignals[groupIdx][bandIdx].load(std::memory_order_relaxed);
        }
        return 0.0f;
    }

private:
    NetworkManager()
    {
        // Инициализируем нулями
        for (auto& group : groupBandSignals)
            for (auto& band : group)
                band.store(0.0f);
    }

    // Двумерный массив атомиков: [Группа][Полоса]
    // Значение = Энергия Reference трека в этой группе и полосе
    std::array<std::array<std::atomic<float>, NUM_BANDS>, MAX_GROUPS> groupBandSignals;
};
