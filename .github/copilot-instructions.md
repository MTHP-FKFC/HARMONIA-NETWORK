# Cohera Saturator Instructions for Copilot

## Architecture & why it is split this way
- `README.md` already describes v1.30 as a Clean Architecture refactor: presentation (`src/PluginProcessor.*`, `PluginEditor.*`), business logic (`src/engine/ProcessingEngine.h` and the sub-engines) and the DSP modules under `src/dsp/`. Keep that layering when you touch code.
- `src/PluginProcessor.cpp` is currently a thin wrapper: it owns the `AudioProcessorValueTreeState`, `ParameterManager`, and the `ProcessingEngine`. The real signal path lives in `ProcessingEngine::processBlockWithDry`, so focus edits there and in the `engine/` subdirectory rather than hacking `PluginProcessor` unless you are wiring new parameters or UI hooks.
- `ProcessingEngine` owns a 4× `juce::dsp::Oversampling`, the `FilterBankEngine`, `MixEngine`, and `NetworkController`. It prepares everything with one `juce::dsp::ProcessSpec` and keeps latency data in sync (`updateLatency`).

## Key layers & components to know
- `ParameterManager` (`src/parameters/ParameterManager.h`) caches `AudioProcessorValueTreeState` raw pointers and turns them into a `ParameterSet` snapshot. `ParameterSet` (`src/parameters/ParameterSet.h`) stores real values (drive 0–100, mix 0–1, output gain in linear, enums from `src/CoheraTypes.h`) plus helpers like `getEffectiveDriveGain()` that other engines expect.
- `FilterBankEngine` (`src/engine/FilterBankEngine.h`) uses the `PlaybackFilterBank` from `src/dsp/FilterBank.{h,cpp}` to split the 6 bands, applies pre-/post-filters and delegates per-band work to `BandProcessingEngine`. The FIR coeffs live in `src/FIR/fir_coeffs_multi_sr.h` and `fir_minphase_128.h`.
- Each `BandProcessingEngine` combines `TransientEngine` (split & crush), `AnalogModelingEngine` (which in turn uses `ThermalModel`, `HarmonicEntropy`, `StereoVariance` from `src/dsp/`), and two `DCBlocker` instances. It multiplies the drive tilt (`kBandTilt` in `FilterBankEngine`) with global heat, analog drift, entropy, noise, etc.
- `MixEngine` uses a `juce::dsp::DelayLine` to keep the dry signal aligned with the processed wet signal after oversampling. Always supply the original dry buffer (see `processBlockWithDry` comments) because `juce::dsp::Oversampling::processSamplesDown` overwrites its input.
- `NetworkController`/`NetworkManager` (`src/network/`) share envelope data between instances: the controller either sends (Reference role) or receives (Listener) per-band modulation and smooths it with `juce::LinearSmoothedValue`. The singleton manager keeps up to 8 groups × 6 bands plus a 64-slot `Global Heat` register.

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

## Wrap-up
- Follow the neon UI guidelines in `src/ui/CoheraLookAndFeel.h` so new controls keep the glow + typography style already used in `SaturationCore` and `NetworkBrain`.
- Consult `README.md` for the “Divine Math” mode descriptions if you need to match sounds to UI names.
- After editing this guidance, flag any new core script/command you rely on so future Copilot sessions stay up to date.

Please flag any section above that feels unclear so I can iterate with you.
