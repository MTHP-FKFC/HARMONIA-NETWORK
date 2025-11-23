#pragma once

#include <array>

namespace Cohera {

/**
 * @brief Interface for Network Manager (Dependency Injection pattern)
 * 
 * This interface allows NetworkController to work with any NetworkManager
 * implementation, making it easier to test and maintain.
 * 
 * The actual NetworkManager is a Singleton (by design!) because it needs
 * to share state between multiple plugin instances in a DAW.
 * 
 * Benefits of this approach:
 * - Testability: Can use MockNetworkManager in unit tests
 * - Loose coupling: NetworkController doesn't depend on Singleton directly
 * - Flexibility: Can swap implementations if needed
 */
class INetworkManager {
public:
    virtual ~INetworkManager() = default;
    
    //==========================================================================
    // BAND SIGNAL API (Per-Group, Per-Band modulation)
    //==========================================================================
    
    /**
     * @brief Reference instances write their band energy to the network
     * @param groupIdx Group ID (0-7)
     * @param bandIdx Band ID (0-5)
     * @param value Envelope value (0.0 - 1.0)
     */
    virtual void updateBandSignal(int groupIdx, int bandIdx, float value) = 0;
    
    /**
     * @brief Listener instances read band energy from the network
     * @param groupIdx Group ID (0-7)
     * @param bandIdx Band ID (0-5)
     * @return Envelope value (0.0 - 1.0)
     */
    virtual float getBandSignal(int groupIdx, int bandIdx) const = 0;
    
    //==========================================================================
    // GLOBAL HEAT API (Instance registration & energy sharing)
    //==========================================================================
    
    /**
     * @brief Register a new plugin instance
     * @return Slot ID (0-63) or -1 if no slots available
     */
    virtual int registerInstance() = 0;
    
    /**
     * @brief Unregister a plugin instance (called in destructor)
     * @param id Slot ID returned by registerInstance()
     */
    virtual void unregisterInstance(int id) = 0;
    
    /**
     * @brief Update instance's energy contribution to global heat
     * @param id Slot ID
     * @param energy RMS energy (0.0+, typically 0.0-1.0)
     */
    virtual void updateInstanceEnergy(int id, float energy) = 0;
    
    /**
     * @brief Get total global heat from all instances
     * @return Sum of all active instances' energy
     */
    virtual float getGlobalHeat() const = 0;
};

} // namespace Cohera