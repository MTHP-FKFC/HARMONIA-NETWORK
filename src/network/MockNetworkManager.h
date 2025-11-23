#pragma once

#include "INetworkManager.h"
#include <array>
#include <mutex>

namespace Cohera {

/**
 * @brief Mock Network Manager for Unit Testing
 * 
 * This is a testable implementation of INetworkManager that can be used
 * in unit tests without relying on the Singleton pattern.
 * 
 * Key differences from real NetworkManager:
 * - Not a Singleton (can create multiple instances)
 * - Uses regular variables instead of atomics (simpler, testable)
 * - Thread-safe with mutex (good enough for tests)
 * - Can be reset between test cases
 * 
 * Usage in tests:
 * ```cpp
 * MockNetworkManager mockNet;
 * NetworkController controller(mockNet);
 * 
 * // Simulate Reference sending data
 * mockNet.updateBandSignal(0, 0, 0.5f);
 * 
 * // Verify Listener receives it
 * EXPECT_FLOAT_EQ(mockNet.getBandSignal(0, 0), 0.5f);
 * ```
 */
class MockNetworkManager : public INetworkManager {
public:
    MockNetworkManager() {
        reset();
    }
    
    ~MockNetworkManager() override = default;
    
    //==========================================================================
    // BAND SIGNAL API
    //==========================================================================
    
    void updateBandSignal(int groupIdx, int bandIdx, float value) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (isValidBandIndex(groupIdx, bandIdx)) {
            bandSignals_[groupIdx][bandIdx] = value;
        }
    }
    
    float getBandSignal(int groupIdx, int bandIdx) const override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (isValidBandIndex(groupIdx, bandIdx)) {
            return bandSignals_[groupIdx][bandIdx];
        }
        return 0.0f;
    }
    
    //==========================================================================
    // GLOBAL HEAT API
    //==========================================================================
    
    int registerInstance() override {
        std::lock_guard<std::mutex> lock(mutex_);
        for (int i = 0; i < MAX_INSTANCES; ++i) {
            if (!slotOccupied_[i]) {
                slotOccupied_[i] = true;
                instanceEnergy_[i] = 0.0f;
                return i;
            }
        }
        return -1; // No slots available
    }
    
    void unregisterInstance(int id) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (isValidInstanceId(id)) {
            slotOccupied_[id] = false;
            instanceEnergy_[id] = 0.0f;
        }
    }
    
    void updateInstanceEnergy(int id, float energy) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (isValidInstanceId(id)) {
            instanceEnergy_[id] = energy;
        }
    }
    
    float getGlobalHeat() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        float total = 0.0f;
        for (int i = 0; i < MAX_INSTANCES; ++i) {
            if (slotOccupied_[i]) {
                total += instanceEnergy_[i];
            }
        }
        return total;
    }
    
    //==========================================================================
    // TEST HELPERS
    //==========================================================================
    
    /**
     * @brief Reset all state (useful between test cases)
     */
    void reset() {
        std::lock_guard<std::mutex> lock(mutex_);
        for (auto& group : bandSignals_) {
            group.fill(0.0f);
        }
        slotOccupied_.fill(false);
        instanceEnergy_.fill(0.0f);
    }
    
    /**
     * @brief Get number of registered instances (for test assertions)
     */
    int getActiveInstanceCount() const {
        std::lock_guard<std::mutex> lock(mutex_);
        int count = 0;
        for (bool occupied : slotOccupied_) {
            if (occupied) ++count;
        }
        return count;
    }
    
    /**
     * @brief Check if instance is registered (for test assertions)
     */
    bool isInstanceRegistered(int id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (isValidInstanceId(id)) {
            return slotOccupied_[id];
        }
        return false;
    }
    
    /**
     * @brief Get instance energy (for test assertions)
     */
    float getInstanceEnergy(int id) const {
        std::lock_guard<std::mutex> lock(mutex_);
        if (isValidInstanceId(id)) {
            return instanceEnergy_[id];
        }
        return 0.0f;
    }

private:
    static constexpr int MAX_GROUPS = 8;
    static constexpr int NUM_BANDS = 6;
    static constexpr int MAX_INSTANCES = 64;
    
    bool isValidBandIndex(int groupIdx, int bandIdx) const {
        return groupIdx >= 0 && groupIdx < MAX_GROUPS &&
               bandIdx >= 0 && bandIdx < NUM_BANDS;
    }
    
    bool isValidInstanceId(int id) const {
        return id >= 0 && id < MAX_INSTANCES;
    }
    
    // State (thread-safe via mutex)
    mutable std::mutex mutex_;
    std::array<std::array<float, NUM_BANDS>, MAX_GROUPS> bandSignals_;
    std::array<bool, MAX_INSTANCES> slotOccupied_;
    std::array<float, MAX_INSTANCES> instanceEnergy_;
};

} // namespace Cohera