# Cohera Saturator Instructions for Copilot

## Architecture & why it is split this way (v1.30 - Post-Refactoring)
- **Clean Architecture** with strict layering: Presentation (`src/PluginProcessor.*`, `PluginEditor.*`), Business Logic (`src/engine/`), DSP (`src/dsp/`), and Data (`src/parameters/`, `src/network/`).
- `src/PluginProcessor.{h,cpp}` is a **thin JUCE wrapper** (174 lines): it owns `AudioProcessorValueTreeState`, `ParameterManager`, and `ProcessingEngine`. The real signal path lives in `ProcessingEngine::processBlockWithDry`.
- **Single Source of Truth**: All DSP state lives in `ProcessingEngine` and its sub-engines. No duplicate modules in `PluginProcessor` (this was cleaned up in v1.30).
- `ProcessingEngine` owns: 4× `juce::dsp::Oversampling`, `FilterBankEngine`, `MixEngine`, and `NetworkController`. It uses **Dependency Injection** to receive `INetworkManager&` in the constructor.

## Key layers & components to know

### 1. Presentation Layer
- `PluginProcessor` is JUCE-only code. Never add DSP logic here—delegate to `ProcessingEngine`.
- All public getters return `const` references or values (no mutable access to internal state).
- Uses atomics (`std::atomic<float>`) for thread-safe UI data access (RMS, transient levels, etc.).

### 2. Business Logic Layer (src/engine/)
- `ParameterManager` (`src/parameters/ParameterManager.h`) caches `AudioProcessorValueTreeState` raw pointers and turns them into a `ParameterSet` snapshot. `ParameterSet` (`src/parameters/ParameterSet.h`) stores real values (drive 0–100, mix 0–1, output gain in linear, enums from `src/CoheraTypes.h`) plus helpers like `getEffectiveDriveGain()` that other engines expect.
- `FilterBankEngine` (`src/engine/FilterBankEngine.h`) uses the `PlaybackFilterBank` from `src/dsp/FilterBank.{h,cpp}` to split the 6 bands, applies pre-/post-filters and delegates per-band work to `BandProcessingEngine`. The FIR coeffs live in `src/FIR/fir_coeffs_multi_sr.h` and `fir_minphase_128.h`.
- Each `BandProcessingEngine` combines `TransientEngine` (split & crush), `AnalogModelingEngine` (which in turn uses `ThermalModel`, `HarmonicEntropy`, `StereoVariance` from `src/dsp/`), and two `DCBlocker` instances. It multiplies the drive tilt (`kBandTilt` in `FilterBankEngine`) with global heat, analog drift, entropy, noise, etc.
- `MixEngine` uses a `juce::dsp::DelayLine` to keep the dry signal aligned with the processed wet signal after oversampling. Always supply the original dry buffer (see `processBlockWithDry` comments) because `juce::dsp::Oversampling::processSamplesDown` overwrites its input.

### 3. Data Layer (src/network/, src/parameters/)
- **Dependency Injection Pattern**: `NetworkController` accepts `INetworkManager&` in constructor (no direct Singleton calls).
- **INetworkManager Interface** (`src/network/INetworkManager.h`) abstracts network storage. Two implementations:
  - `NetworkManager` (real, Singleton): Shares state between plugin instances in DAW
  - `MockNetworkManager` (test): Isolated mock for unit tests
- `NetworkController` either sends (Reference role) or receives (Listener) per-band modulation and smooths it with `juce::LinearSmoothedValue`. The manager keeps up to 8 groups × 6 bands plus a 64-slot `Global Heat` register.
- **Why Singleton?**: Multiple plugin instances MUST share state to communicate. This is a feature, not a bug. But it's injected via interface for testability.

## Parameter & UI conventions
- `CoheraTypes.h` defines enums for `SaturationMode`, `NetworkMode`, `QualityMode`, and `NetworkRole`. All UI controls use those enums and the `math_mode`/`mode` parameter IDs, so reference those names when adding bindings.
- The UI (`src/ui/`) uses `juce::AudioProcessorValueTreeState` attachments inside components like `SaturationCore` and `NetworkBrain`, so new knobs or combo boxes should follow that pattern and rely on `CoheraLookAndFeel` for the neon styling.
- `NetworkBrain` visualizes live `NetworkManager` values (group, role, mode and meter), so reuse its `InteractionMeter` logic when reproducing network feedback in other views.

## Build, install, and validation workflows
- Primary build script: run `./build_plugin.sh`. It wipes `build/`, runs `cmake ..`, `make Cohera_Saturator_Standalone -j4` and, as a best-effort, `make Cohera_Saturator_VST3 -j4` (this last target often fails because JUCE/parameter conflicts are expected). You can also create `build/` manually and run `cmake ..` followed by whichever targets you need.
- After a successful build, `./install_plugin.sh` copies the VST3/AU binaries from `build/Cohera_Saturator_artefacts` into `~/Library/Audio/Plug-Ins/...`, while `./install_release_plugins.sh` copies from the `Release` subtree. Use `check_plugins.sh` and `test_plugin.sh` to verify the binaries in the DAW folders and to follow the Ableton rescanning tips they echo.
- `run_tests.sh` is a lightweight gate that ensures source files and the expected VST3 binary exist; it also lists how to run the full integration tests: rebuild in Debug (`cmake .. && make -j4`), launch the standalone app (`build/Cohera_Saturator_artefacts/Standalone/Cohera Saturator.app/Contents/MacOS/Cohera Saturator`), and monitor console logs.
- Always run `validate_architecture.sh` after touching core `engine/`, `dsp/`, or `network/` headers—its checks ensure the right includes exist (`ProcessingEngine`, `FilterBankEngine`, `NetworkController`, etc.) and that every key file still lives under the `Cohera` namespace.
- CMake hardcodes `add_subdirectory(/Users/macos/JUCE JUCE)`, so update that path to your local JUCE checkout before configuring.

## Testing targets you can run directly
- `make Cohera_Tests` produces `build/Cohera_Saturator_artefacts/Tests/Cohera_Tests` and runs `src/tests/TestRunner.cpp` plus `EngineIntegrationTests.cpp`/`RealWorldScenarios.cpp`. It links against the DSP modules without UI to check the processing pipeline in isolation.
- `make IndustryStandardTest` (output: `build/IndustryStandardTest`) runs `src/tests/IndustryStandardTests.cpp`; it instantiates `PluginProcessor`, randomizes parameters via `APVTS`, and asserts state recall, variable block size robustness, and smoothing.
- `make SimpleDSPTest` quickly builds `src/tests/SimpleTestRunner.cpp` which touches only `MathSaturator`, `DCBlocker`, and `CoheraTypes`, so it runs faster when you just need to sanity-check the low-level math.

## Real-time gotchas & recurring patterns
- `ProcessingEngine::processBlockWithDry` is the intended real-time path. Right now `CoheraSaturatorAudioProcessor::processBlock` only measures RMS and never calls it (the call is commented out to avoid a segmentation fault). When you re-enable it, copy the dry buffer before oversampling; `MixEngine` expects a clean reference so the delay line can align Dry/Wet instead of trying to reconstruct the dry signal after `processSamplesDown` overwrote it.
- Network data must stay lock-free: `NetworkManager::updateBandSignal`/`getBandSignal` use relaxed atomics, and `registerInstance`/`unregisterInstance` gate `Global Heat`. Don’t introduce blocking operations in `NetworkController::process`.
- DSP helpers under `src/dsp/` (e.g., `ThermalModel`, `HarmonicEntropy`, `StereoVariance`, `TransientSplitter`) follow the `prepare()/reset()/process()` pattern that all other engines adopt; add new modules the same way so the engine code stays predictable.

## OOP Best Practices (Enforced in v1.30)
1. **Single Responsibility**: Each class does ONE thing. If a class grows beyond 300 lines, consider splitting.
2. **Dependency Injection**: Pass dependencies via constructor, not Singleton calls. Example: `ProcessingEngine(INetworkManager&)`.
3. **Const Correctness**: Getters return `const&` or values. Never return mutable references unless absolutely needed.
4. **Interface Segregation**: Use abstract interfaces (`INetworkManager`) for components that need to be testable or swappable.
5. **No Duplicate State**: If `ProcessingEngine` owns a module, `PluginProcessor` must NOT own a duplicate. Trust the engine.

## Testing Strategy
- **Unit Tests**: Use `MockNetworkManager` to isolate components. See `src/tests/EngineIntegrationTests.cpp` for examples.
- **Integration Tests**: Use real `NetworkManager` to test multi-instance scenarios. See `src/tests/RealWorldScenarios.cpp`.
- Always run `./validate_architecture.sh` after touching core modules to ensure dependencies are correct.

## New Files Added in v1.30 Refactoring
- `src/network/INetworkManager.h` - Abstract interface for network storage
- `src/network/MockNetworkManager.h` - Test mock implementation
- `ARCHITECTURE.md` - Full architecture documentation (read this first!)

## Wrap-up
- Follow the neon UI guidelines in `src/ui/CoheraLookAndFeel.h` so new controls keep the glow + typography style already used in `SaturationCore` and `NetworkBrain`.
- Consult `README.md` for the "Divine Math" mode descriptions if you need to match sounds to UI names.
- **Before making architectural changes**, read `ARCHITECTURE.md` to understand the layering and OOP principles.
- After editing this guidance, flag any new core script/command you rely on so future Copilot sessions stay up to date.

Please flag any section above that feels unclear so I can iterate with you.
