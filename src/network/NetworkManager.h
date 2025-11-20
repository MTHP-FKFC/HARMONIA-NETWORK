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

    // === НОВАЯ СИСТЕМА: GLOBAL HEAT ===

    // 1. Регистрация: плагин просит слот
    // Возвращает ID слота (0..63) или -1, если места нет
    int registerInstance()
    {
        for (int i = 0; i < MAX_INSTANCES; ++i)
        {
            bool expected = false;
            // Пытаемся занять слот (CAS операция)
            if (slotOccupied[i].compare_exchange_strong(expected, true))
            {
                instanceEnergy[i].store(0.0f);
                return i;
            }
        }
        return -1;
    }

    // 2. Удаление: плагин освобождает слот
    void unregisterInstance(int id)
    {
        if (id >= 0 && id < MAX_INSTANCES)
        {
            instanceEnergy[id].store(0.0f);
            slotOccupied[id].store(false);
        }
    }

    // 3. Обновление: плагин сообщает свою энергию
    void updateInstanceEnergy(int id, float energy)
    {
        if (id >= 0 && id < MAX_INSTANCES)
        {
            instanceEnergy[id].store(energy, std::memory_order_relaxed);
        }
    }

    // 4. Чтение: плагин узнает общую температуру
    // (Суммируем все активные слоты)
    float getGlobalHeat() const
    {
        float total = 0.0f;
        for (int i = 0; i < MAX_INSTANCES; ++i)
        {
            // Читаем relaxed для скорости, нам не нужна идеальная синхронизация
            if (slotOccupied[i].load(std::memory_order_relaxed))
            {
                total += instanceEnergy[i].load(std::memory_order_relaxed);
            }
        }
        // Возвращаем сырую сумму
        // 1.0 = один трек в 0dB, 10.0 = десять треков и т.д.
        return total;
    }

private:
    NetworkManager()
    {
        // Инициализируем нулями старые массивы
        for (auto& group : groupBandSignals)
            for (auto& band : group)
                band.store(0.0f);

        // Инициализируем новые массивы для Heat
        for (auto& slot : slotOccupied) slot.store(false);
        for (auto& en : instanceEnergy) en.store(0.0f);
    }

    // Старые массивы
    std::array<std::array<std::atomic<float>, NUM_BANDS>, MAX_GROUPS> groupBandSignals;

    // Новые массивы для Global Heat
    std::array<std::atomic<bool>, MAX_INSTANCES> slotOccupied;
    std::array<std::atomic<float>, MAX_INSTANCES> instanceEnergy;
};
