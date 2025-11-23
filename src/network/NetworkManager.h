#pragma once

#include "INetworkManager.h"
#include <array>
#include <atomic>

namespace Cohera {

// Максимальное кол-во инстансов (с запасом)
static constexpr int MAX_INSTANCES = 64;
// Кол-во групп (0..7)
static constexpr int MAX_GROUPS = 8;
static constexpr int NUM_BANDS = 6; // Наши 6 полос

/**
 * @brief Network Manager - Shared state between plugin instances
 * 
 * This is a Singleton by design! Multiple plugin instances in a DAW
 * need to share the same state to communicate with each other.
 * 
 * Architecture:
 * - Reference instances write their band energy to the network
 * - Listener instances read and react to that energy
 * - Global Heat: all instances contribute to a shared "temperature"
 * 
 * Thread Safety:
 * - Uses std::atomic with relaxed memory order for speed
 * - Lock-free design (safe for real-time audio thread)
 * 
 * Usage (via Dependency Injection):
 * ```cpp
 * INetworkManager& netMgr = NetworkManager::getInstance();
 * NetworkController controller(netMgr);
 * ```
 */
class NetworkManager : public INetworkManager
{
public:
    // Singleton access (thread-safe in C++11+)
    static NetworkManager& getInstance()
    {
        static NetworkManager instance;
        return instance;
    }

    // Delete copy/move (Singleton pattern)
    NetworkManager(const NetworkManager&) = delete;
    NetworkManager& operator=(const NetworkManager&) = delete;
    NetworkManager(NetworkManager&&) = delete;
    NetworkManager& operator=(NetworkManager&&) = delete;

    //==========================================================================
    // BAND SIGNAL API (Per-Group, Per-Band modulation)
    //==========================================================================

    void updateBandSignal(int groupIdx, int bandIdx, float value) override
    {
        if (groupIdx >= 0 && groupIdx < MAX_GROUPS && bandIdx >= 0 && bandIdx < NUM_BANDS)
        {
            // CRITICAL FIX: Use release semantics to ensure visibility across threads
            groupBandSignals[groupIdx][bandIdx].store(value, std::memory_order_release);
        }
    }

    float getBandSignal(int groupIdx, int bandIdx) const override
    {
        if (groupIdx >= 0 && groupIdx < MAX_GROUPS && bandIdx >= 0 && bandIdx < NUM_BANDS)
        {
            // CRITICAL FIX: Use acquire semantics to see all prior release operations
            return groupBandSignals[groupIdx][bandIdx].load(std::memory_order_acquire);
        }
        return 0.0f;
    }

    //==========================================================================
    // GLOBAL HEAT API (Instance registration & energy sharing)
    //==========================================================================

    int registerInstance() override
    {
        for (int i = 0; i < MAX_INSTANCES; ++i)
        {
            bool expected = false;
            // CRITICAL FIX: Use acquire/release semantics to prevent ABA problem
            // Acquire ensures we see all previous operations before claiming slot
            if (slotOccupied[i].compare_exchange_strong(expected, true,
                std::memory_order_acquire, std::memory_order_relaxed))
            {
                // CRITICAL FIX: Initialize energy atomically with release semantics
                // This ensures the initialization is visible before slot is marked as occupied
                instanceEnergy[i].store(0.0f, std::memory_order_release);
                return i;
            }
        }
        return -1; // No slots available
    }

    void unregisterInstance(int id) override
    {
        if (id >= 0 && id < MAX_INSTANCES)
        {
            // CRITICAL FIX: Clear energy BEFORE releasing slot to prevent ABA race
            // Use release semantics to ensure clearing is visible
            instanceEnergy[id].store(0.0f, std::memory_order_release);
            slotOccupied[id].store(false, std::memory_order_release);
        }
    }

    void updateInstanceEnergy(int id, float energy) override
    {
        if (id >= 0 && id < MAX_INSTANCES)
        {
            // CRITICAL FIX: Use release semantics for proper visibility
            instanceEnergy[id].store(energy, std::memory_order_release);
        }
    }

    float getGlobalHeat() const override
    {
        float total = 0.0f;
        for (int i = 0; i < MAX_INSTANCES; ++i)
        {
            // CRITICAL FIX: Use acquire semantics to see latest energy values
            if (slotOccupied[i].load(std::memory_order_acquire))
            {
                total += instanceEnergy[i].load(std::memory_order_acquire);
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

    ~NetworkManager() override = default;

    //==========================================================================
    // SHARED STATE (Lock-free atomics)
    //==========================================================================

    // Band signals: [Group 0-7][Band 0-5]
    std::array<std::array<std::atomic<float>, NUM_BANDS>, MAX_GROUPS> groupBandSignals;

    // Global Heat: [Instance 0-63]
    std::array<std::atomic<bool>, MAX_INSTANCES> slotOccupied;
    std::array<std::atomic<float>, MAX_INSTANCES> instanceEnergy;
};

} // namespace Cohera