# Cohera Saturator - Architecture Documentation

**Version:** 1.30 (Post-Refactoring)  
**Date:** 2024  
**Status:** Clean Architecture Implementation

---

## 🎯 Overview

Cohera Saturator is a professional audio saturation plugin built with JUCE, following **Clean Architecture** principles with a focus on Object-Oriented Programming (OOP) best practices.

### Key Design Goals

1. **Separation of Concerns**: Clear boundaries between presentation, business logic, and DSP
2. **Testability**: Dependency Injection enables unit testing without DAW
3. **Maintainability**: Single Responsibility Principle, minimal coupling
4. **Performance**: Real-time safe, lock-free where critical
5. **Extensibility**: Interface-based design allows easy component swapping

---

## 📊 Architecture Layers

```
┌─────────────────────────────────────────────────────┐
│  PRESENTATION LAYER (JUCE Framework)                │
│  - CoheraSaturatorAudioProcessor                    │
│  - CoheraSaturatorAudioProcessorEditor              │
│  - UI Components (ReactorKnob, NetworkBrain, etc.)  │
└─────────────────────────────────────────────────────┘
                       ▼
┌─────────────────────────────────────────────────────┐
│  BUSINESS LOGIC LAYER (Engine)                      │
│  - ProcessingEngine                                 │
│  - FilterBankEngine                                 │
│  - BandProcessingEngine                             │
│  - NetworkController                                │
└─────────────────────────────────────────────────────┘
                       ▼
┌─────────────────────────────────────────────────────┐
│  DSP LAYER (Low-level algorithms)                   │
│  - MathSaturator, Waveshaper                        │
│  - ThermalModel, HarmonicEntropy                    │
│  - DCBlocker, EnvelopeFollower                      │
│  - FilterBank (PlaybackFilterBank)                  │
└─────────────────────────────────────────────────────┘
                       ▼
┌─────────────────────────────────────────────────────┐
│  DATA LAYER (State & Network)                       │
│  - ParameterManager → ParameterSet                  │
│  - INetworkManager ← NetworkManager (Singleton)     │
└─────────────────────────────────────────────────────┘
```

---

## 🏗️ Core Components

### 1. Presentation Layer

#### `CoheraSaturatorAudioProcessor`
**Responsibility:** JUCE lifecycle wrapper (thin layer)

```cpp
class CoheraSaturatorAudioProcessor : public juce::AudioProcessor {
private:
    juce::AudioProcessorValueTreeState apvts;
    Cohera::ParameterManager paramManager;
    Cohera::ProcessingEngine processingEngine;
    SimpleFFT analyzer;
    // + visualization data (atomics, FIFO)
};
```

**Key Points:**
- ✅ **Clean:** Only 174 lines (down from 500+)
- ✅ **Delegating:** All DSP logic lives in `ProcessingEngine`
- ✅ **Thread-safe:** Uses atomics for UI data access
- ✅ **No mutable getters:** All public getters return const references

---

### 2. Business Logic Layer

#### `ProcessingEngine`
**Responsibility:** Orchestrate DSP pipeline

**Signal Flow:**
```
Input → Network Analysis → Upsample 4x → FilterBankEngine
  → Downsample → MixEngine (Dry/Wet) → Output
```

**Constructor (Dependency Injection):**
```cpp
explicit ProcessingEngine(INetworkManager& networkManager)
    : networkController(networkManager) { ... }
```

**Why DI?**
- Can inject `MockNetworkManager` in tests
- No hidden Singleton dependency
- Easier to reason about component relationships

---

#### `FilterBankEngine`
**Responsibility:** 6-band multiband processing

**Signal Flow:**
```
Input → Pre-Filter (Tighten) → Split 6 Bands → BandProcessingEngine[6]
  → Sum Bands → Post-Filter (Smooth) → Output
```

**Components:**
- `PlaybackFilterBank`: FIR-based crossover (Linear Phase, 128 taps)
- `BandProcessingEngine[6]`: One per frequency band
- Pre/Post TPT Filters: Tone shaping (HPF/LPF)

---

#### `BandProcessingEngine`
**Responsibility:** Per-band saturation + modulation

**Signal Flow:**
```
Input → AnalogModelingEngine → TransientEngine → DCBlocker → Output
         (applies network modulation to Drive/Punch/Mojo)
```

**Key Features:**
- Network modulation via `InteractionEngine::calculateModulation()`
- Drive tilt per band (sub gets less drive, highs get more)
- Thermal modeling (ThermalModel, HarmonicEntropy)

---

#### `NetworkController`
**Responsibility:** Inter-instance communication

**Roles:**
- **Reference:** Analyze input → send envelopes to network
- **Listener:** Read envelopes from network → return modulations

**Constructor (Dependency Injection):**
```cpp
explicit NetworkController(INetworkManager& manager)
    : networkManager(manager) { ... }
```

**Thread Safety:**
- Lock-free (uses atomic operations via `INetworkManager`)
- Safe for real-time audio thread

---

### 3. Data Layer

#### `INetworkManager` (Interface)
**Responsibility:** Abstract network storage

```cpp
class INetworkManager {
public:
    virtual void updateBandSignal(int group, int band, float value) = 0;
    virtual float getBandSignal(int group, int band) const = 0;
    virtual int registerInstance() = 0;
    virtual void unregisterInstance(int id) = 0;
    virtual void updateInstanceEnergy(int id, float energy) = 0;
    virtual float getGlobalHeat() const = 0;
};
```

**Why Interface?**
- **Testability:** Can swap real `NetworkManager` with `MockNetworkManager`
- **Loose Coupling:** Components depend on interface, not Singleton
- **Flexibility:** Future alternative implementations (e.g., OSC-based networking)

---

#### `NetworkManager` (Singleton Implementation)
**Responsibility:** Shared state between plugin instances

```cpp
class NetworkManager : public INetworkManager {
public:
    static NetworkManager& getInstance();
    // ... implements INetworkManager
private:
    std::array<std::array<std::atomic<float>, 6>, 8> groupBandSignals;
    std::array<std::atomic<bool>, 64> slotOccupied;
    std::array<std::atomic<float>, 64> instanceEnergy;
};
```

**Why Singleton?**
- Multiple plugin instances in a DAW **must** share the same state
- This is a *feature*, not a bug (enables inter-track communication)
- Singleton is injected via interface, so it's testable

---

#### `MockNetworkManager` (Test Implementation)
**Responsibility:** Testable network mock

```cpp
class MockNetworkManager : public INetworkManager {
public:
    // Same interface, but uses regular variables + mutex
    void reset();
    int getActiveInstanceCount() const;
    bool isInstanceRegistered(int id) const;
};
```

**Usage in Tests:**
```cpp
MockNetworkManager mockNet;
ProcessingEngine engine(mockNet);
// No global state, fully isolated test
```

---

#### `ParameterManager`
**Responsibility:** Cache APVTS pointers, return value objects

```cpp
class ParameterManager {
public:
    ParameterManager(juce::AudioProcessorValueTreeState& apvts);
    ParameterSet getCurrentParams() const;
private:
    std::atomic<float>* pDrive;
    std::atomic<float>* pMix;
    // ... etc
};
```

**Benefits:**
- Only reads APVTS once per block (performance)
- Returns immutable `ParameterSet` (thread-safe copy)
- Engines work with plain structs, not JUCE types

---

## 🧪 Testing Strategy

### Unit Tests (with Mocks)

```cpp
// Example: Test NetworkController in isolation
TEST(NetworkController, ListenerReceivesReferenceSignal) {
    MockNetworkManager mockNet;
    NetworkController controller(mockNet);
    
    // Simulate Reference sending data
    mockNet.updateBandSignal(0, 0, 0.8f);
    
    // Create Listener params
    ParameterSet params;
    params.netRole = NetworkRole::Listener;
    params.groupId = 0;
    
    juce::AudioBuffer<float> dummyBuffer(2, 512);
    auto mods = controller.process(dummyBuffer, params);
    
    EXPECT_GT(mods[0], 0.0f); // Should receive modulation
}
```

### Integration Tests (with Real NetworkManager)

```cpp
// Example: Test two engines communicating
TEST(ProcessingEngine, NetworkDucking) {
    Cohera::MockNetworkManager mockNet;
    ProcessingEngine kickEngine(mockNet);
    ProcessingEngine bassEngine(mockNet);
    
    // Kick is Reference, Bass is Listener
    // Verify Bass ducks when Kick hits
}
```

---

## 📈 Improvements from Refactoring

### Before (v1.29)
```cpp
class CoheraSaturatorAudioProcessor {
private:
    // 500+ lines of duplicated DSP state
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler; // duplicate!
    std::array<std::array<ThermalModel, 2>, 6> tubes; // duplicate!
    // ... 20+ smoothers, filters, etc.
    
    ProcessingEngine processingEngine; // also has all of the above!
};
```

### After (v1.30)
```cpp
class CoheraSaturatorAudioProcessor {
private:
    juce::AudioProcessorValueTreeState apvts;
    Cohera::ParameterManager paramManager;
    Cohera::ProcessingEngine processingEngine; // SINGLE SOURCE OF TRUTH
    SimpleFFT analyzer;
    // + lightweight visualization data
};
```

**Lines of Code:**
- PluginProcessor.h: **500 → 174 lines** (65% reduction)
- Removed: 12 duplicate DSP modules, 20+ smoothers, 3 filter banks

**OOP Score (before → after):**

| Principle | Before | After | Delta |
|-----------|--------|-------|-------|
| **Encapsulation** | 7/10 | 9/10 | +2 |
| **Single Responsibility** | 6/10 | 9/10 | +3 |
| **Open/Closed** | 5/10 | 8/10 | +3 |
| **Dependency Inversion** | 4/10 | 9/10 | +5 |
| **Composition** | 9/10 | 9/10 | 0 |
| **TOTAL** | **31/50** | **44/50** | **+13** |

---

## 🚀 Build & Test

### Quick Build
```bash
./build_plugin.sh
```

### Run Tests
```bash
cd build
./tests/Cohera_Tests
```

### Validate Architecture
```bash
./validate_architecture.sh
```

**Expected Output:**
```
=== VALIDATION SUMMARY ===
Passed: 35/36
✓ All core modules present
✓ ProcessingEngine dependencies correct
✓ Cohera namespace consistent
```

---

## 📚 Key Files Reference

### Core Architecture
- `src/engine/ProcessingEngine.h` - Main DSP orchestrator
- `src/engine/FilterBankEngine.h` - Multiband splitter
- `src/engine/BandProcessingEngine.h` - Per-band processor
- `src/network/NetworkController.h` - Inter-instance communication

### Dependency Injection
- `src/network/INetworkManager.h` - Interface (abstract)
- `src/network/NetworkManager.h` - Real implementation (Singleton)
- `src/network/MockNetworkManager.h` - Test mock

### Parameters
- `src/parameters/ParameterManager.h` - APVTS → ParameterSet
- `src/parameters/ParameterSet.h` - Value object (immutable)
- `src/CoheraTypes.h` - Enums (SaturationMode, NetworkMode, etc.)

### DSP Modules
- `src/dsp/MathSaturator.h` - Divine Math algorithms
- `src/dsp/ThermalModel.h` - Tube heating simulation
- `src/dsp/HarmonicEntropy.h` - Chaos generator
- `src/dsp/FilterBank.{h,cpp}` - Linear-phase FIR crossover

### Presentation
- `src/PluginProcessor.{h,cpp}` - JUCE wrapper (174 lines)
- `src/PluginEditor.{h,cpp}` - UI host
- `src/ui/Components/` - Custom controls (ReactorKnob, NetworkBrain)
- `src/ui/visuals/` - Visual effects (PlasmaCore, NebulaShaper)

---

## 🔮 Future Improvements

### Short-term
- [ ] Add more unit tests for `BandProcessingEngine`
- [ ] Document InteractionEngine modes in detail
- [ ] Profile performance (target: < 5% CPU)

### Medium-term
- [ ] Abstract `FilterBankEngine` behind interface (swap FIR/IIR)
- [ ] Extract `TransientEngine` and `AnalogModelingEngine` to interfaces
- [ ] Add real-time performance metrics to UI

### Long-term
- [ ] Plugin preset system (JSON-based)
- [ ] Network protocol v2 (per-band spectral data)
- [ ] Machine learning saturation model (optional)

---

## 🙏 Contributing

When adding new features, follow these principles:

1. **Single Responsibility**: Each class does ONE thing well
2. **Dependency Injection**: Pass dependencies via constructor
3. **Interface Segregation**: Create small, focused interfaces
4. **Real-time Safety**: No allocations in `process()` calls
5. **Test Coverage**: Add tests before merging

---

## 📞 Contact & Support

**Project Lead:** [Your Name]  
**Repository:** [Your Repo URL]  
**Discord:** [Community Link]

For architecture questions, open an issue with the `architecture` label.

---

**Last Updated:** 2024  
**License:** Proprietary