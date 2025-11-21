#pragma once

#include <array>
#include <atomic>

// Максимальное кол-во инстансов (с запасом)
static constexpr int MAX_INSTANCES = 64;
// Кол-во групп (0..7)
static constexpr int MAX_GROUPS = 8;

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

    // 1. Reference пишет свою громкость
    // groupIdx: 0-7
    // value: 0.0 - 1.0 (Envelope)
    void updateGroupSignal(int groupIdx, float value)
    {
        if (groupIdx >= 0 && groupIdx < MAX_GROUPS)
        {
            // store(memory_order_relaxed) - самая быстрая операция
            groupSignals[groupIdx].store(value, std::memory_order_relaxed);
        }
    }

    // 2. Listener читает громкость группы
    float getGroupSignal(int groupIdx) const
    {
        if (groupIdx >= 0 && groupIdx < MAX_GROUPS)
        {
            return groupSignals[groupIdx].load(std::memory_order_relaxed);
        }
        return 0.0f;
    }

private:
    NetworkManager()
    {
        // Инициализируем нулями
        for (auto& val : groupSignals) val.store(0.0f);
    }

    // Таблица сигналов.
    // Индекс массива = ID группы.
    // Значение = Громкость Reference трека в этой группе.
    std::array<std::atomic<float>, MAX_GROUPS> groupSignals;
};
