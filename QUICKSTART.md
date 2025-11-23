# Cohera Saturator - Quick Start Guide

**Version:** 1.30  
**For:** Developers joining the project  
**Read Time:** 5 minutes

---

## 🚀 Get Started in 30 Seconds

```bash
# Clone and build
cd Cohera_Saturator
./build_plugin.sh

# Run tests
cd build && ./tests/Cohera_Tests

# Install to DAW
cd .. && ./install_plugin.sh
```

**Done!** Plugin is now in your Audio Units/VST3 folder.

---

## 📁 Project Structure

```
Cohera_Saturator/
├── src/
│   ├── PluginProcessor.{h,cpp}      # JUCE wrapper (174 lines)
│   ├── PluginEditor.{h,cpp}         # UI host
│   ├── engine/                      # Business logic
│   │   ├── ProcessingEngine.h       # Main DSP orchestrator
│   │   ├── FilterBankEngine.h       # 6-band splitter
│   │   └── BandProcessingEngine.h   # Per-band saturation
│   ├── dsp/                         # Low-level algorithms
│   │   ├── MathSaturator.h          # Divine Math modes
│   │   ├── ThermalModel.h           # Tube simulation
│   │   └── FilterBank.{h,cpp}       # FIR crossover
│   ├── network/                     # Inter-instance comms
│   │   ├── INetworkManager.h        # Interface (DI)
│   │   ├── NetworkManager.h         # Singleton (real)
│   │   └── MockNetworkManager.h     # Test mock
│   ├── parameters/                  # State management
│   │   ├── ParameterManager.h       # APVTS → ParameterSet
│   │   └── ParameterSet.h           # Value object
│   └── ui/                          # Custom controls
│       ├── Components/              # Knobs, meters
│       └── visuals/                 # Visual effects
├── tests/                           # Integration tests
├── ARCHITECTURE.md                  # Full documentation
├── REFACTORING_REPORT.md            # OOP improvements log
└── .github/copilot-instructions.md  # AI assistant guide
```

---

## 🎯 Architecture at a Glance

### Clean Architecture Layers

```
Presentation → Business Logic → DSP → Data
    ↓              ↓              ↓      ↓
PluginProcessor → ProcessingEngine → MathSaturator → ParameterSet
                                        ThermalModel    NetworkManager
```

### Key Principle: **Single Source of Truth**

- ❌ **DON'T:** Add DSP state to `PluginProcessor`
- ✅ **DO:** Add it to `ProcessingEngine` or sub-engines

### Dependency Injection Example

```cpp
// DON'T: Direct Singleton call
NetworkManager::getInstance().updateBandSignal(...);

// DO: Inject interface
class NetworkController {
    explicit NetworkController(INetworkManager& manager);
};

// Usage:
INetworkManager& netMgr = NetworkManager::getInstance();
NetworkController controller(netMgr); // Testable!
```

---

## 🧪 Testing

### Run Unit Tests

```bash
cd build
./tests/Cohera_Tests
```

### Write a Test (Example)

```cpp
#include "../network/MockNetworkManager.h"

TEST(ProcessingEngine, ProcessesSilence) {
    // Arrange
    Cohera::MockNetworkManager mockNet;
    Cohera::ProcessingEngine engine(mockNet);
    engine.prepare({44100.0, 512, 2});
    
    // Act
    juce::AudioBuffer<float> buffer(2, 512);
    buffer.clear();
    // ... process buffer
    
    // Assert
    EXPECT_LT(buffer.getMagnitude(0, 512), 0.01f);
}
```

**Key:** Use `MockNetworkManager` for isolated tests!

---

## 🔧 Common Tasks

### Add a New DSP Module

1. Create `src/dsp/MyModule.h`
2. Follow `prepare()` / `reset()` / `process()` pattern
3. Add to appropriate engine (e.g., `BandProcessingEngine`)
4. Update `CMakeLists.txt` if needed
5. Write unit test in `src/tests/`

**Example:**

```cpp
class MyModule {
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {
        sampleRate = spec.sampleRate;
    }
    
    void reset() {
        state = 0.0f;
    }
    
    float process(float input) {
        // Your DSP here
        return input * gain;
    }
    
private:
    double sampleRate = 44100.0;
    float state = 0.0f;
    float gain = 1.0f;
};
```

---

### Add a New Parameter

1. **Add to `PluginProcessor::createParameterLayout()`**

```cpp
layout.add(std::make_unique<juce::AudioParameterFloat>(
    "my_param", "My Param",
    juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
    50.0f)); // default
```

2. **Cache in `ParameterManager` constructor**

```cpp
pMyParam = apvts.getRawParameterValue("my_param");
```

3. **Add to `ParameterSet` struct**

```cpp
struct ParameterSet {
    float myParam = 50.0f;
    // ...
};
```

4. **Read in `ParameterManager::getCurrentParams()`**

```cpp
params.myParam = pMyParam->load() / 100.0f; // normalize
```

5. **Use in engine**

```cpp
void ProcessingEngine::process(..., const ParameterSet& params) {
    float value = params.myParam;
    // ...
}
```

---

### Debug a Specific Module

```bash
# Build with debug symbols
cd build
cmake -DCMAKE_BUILD_TYPE=Debug ..
make Cohera_Saturator_Standalone -j4

# Launch debugger (Xcode/LLDB)
lldb "./Cohera_Saturator_artefacts/Debug/Standalone/Cohera Saturator.app"
(lldb) run

# Or use Xcode:
open Cohera_Saturator.xcodeproj
```

**Tip:** Add logging in `ProcessingEngine::processBlockWithDry()`:

```cpp
juce::Logger::writeToLog("Drive: " + juce::String(params.drive));
```

---

## 🚨 Before You Commit

### Checklist

```bash
# 1. Build succeeds
./build_plugin.sh

# 2. Tests pass
cd build && ./tests/Cohera_Tests

# 3. Architecture valid
./validate_architecture.sh

# 4. No new warnings
cd build && make 2>&1 | grep warning

# 5. Code formatted (if using clang-format)
clang-format -i src/**/*.{h,cpp}
```

---

## 📚 Learn More

| Topic | File |
|-------|------|
| **Full Architecture** | `ARCHITECTURE.md` |
| **OOP Principles** | `REFACTORING_REPORT.md` |
| **AI Assistant Guide** | `.github/copilot-instructions.md` |
| **Signal Flow** | `README.md` (section "How It Works") |
| **Network Modes** | `src/dsp/InteractionEngine.h` |
| **Test Patterns** | `src/tests/EngineIntegrationTests.cpp` |

---

## 🐛 Troubleshooting

### Build fails: "JUCE not found"

```bash
# Update CMakeLists.txt line 11:
add_subdirectory(/YOUR/PATH/TO/JUCE JUCE)
```

### Tests fail: "NetworkManager instance error"

Use `MockNetworkManager`, not `NetworkManager::getInstance()` in tests!

```cpp
// ❌ WRONG:
Cohera::ProcessingEngine engine;

// ✅ CORRECT:
Cohera::MockNetworkManager mockNet;
Cohera::ProcessingEngine engine(mockNet);
```

### VST3 signature warning

Normal for dev builds. Ignore or use:

```bash
codesign --force --deep --sign - "build/.../Cohera Saturator.vst3"
```

---

## 💡 Pro Tips

1. **Read `ARCHITECTURE.md` first** - saves hours of confusion
2. **Use MockNetworkManager** - makes tests 10x easier
3. **Follow the `prepare()/reset()/process()` pattern** - keeps DSP modules consistent
4. **Check `validate_architecture.sh`** - catches broken dependencies early
5. **Const-correctness matters** - getters should return `const&` when possible

---

## 🎵 Example: Add Stereo Width Control

```cpp
// 1. Add parameter (PluginProcessor.cpp)
layout.add(std::make_unique<juce::AudioParameterFloat>(
    "width", "Stereo Width",
    juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f), 100.0f));

// 2. Cache in ParameterManager
pWidth = apvts.getRawParameterValue("width");

// 3. Add to ParameterSet
struct ParameterSet {
    float width = 1.0f; // 0.0 = mono, 1.0 = normal, 2.0 = super wide
};

// 4. Read in getCurrentParams()
params.width = pWidth->load() / 100.0f;

// 5. Use in MixEngine
void MixEngine::process(..., float width) {
    // M/S processing
    float mid = (left + right) * 0.5f;
    float side = (left - right) * 0.5f * width;
    left = mid + side;
    right = mid - side;
}
```

---

## 📞 Need Help?

1. Check `ARCHITECTURE.md` (comprehensive guide)
2. Search existing issues on GitHub
3. Ask in Discord/Slack (if available)
4. Open an issue with `[question]` tag

---

**Ready to code? Start with `ARCHITECTURE.md` for the deep dive! 🚀**

**Version:** 1.30  
**Last Updated:** 2024