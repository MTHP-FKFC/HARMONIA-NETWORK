cmake_minimum_required(VERSION 3.15)

project(Cohera_Saturator VERSION 0.1.0)

# 1. Подключаем JUCE (предполагаем, что он лежит рядом или установлен глобально)
# Если используешь подмодуль git: add_subdirectory(JUCE)
add_subdirectory(/Users/macos/JUCE JUCE)

# Используем legacy parameter IDs чтобы избежать конфликтов VST2/VST3
add_compile_definitions(JUCE_FORCE_USE_LEGACY_PARAM_IDS=1)

# Строгие предупреждения для поиска мертвого кода
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
    add_compile_options(-Wall -Wextra -Werror=unused-variable -Werror=unused-function -Werror=unused-private-field)
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    add_compile_options(/W4)
endif()

# 2. Создаем цель плагина
juce_add_plugin(Cohera_Saturator
    COMPANY_NAME "CoheraAudio"
    BUNDLE_ID "com.coheraaudio.coherasaturator"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    # Форматы - VST3 + AU для максимальной совместимости
    FORMATS AU VST3 Standalone
    # Название продукта
    PRODUCT_NAME "Cohera Saturator"
    plugin_name "Cohera Saturator"
)

# 3. Подключаем исходники (Явно, чтобы контролировать структуру)
target_sources(Cohera_Saturator PRIVATE
    src/Main.cpp
    src/PluginProcessor.cpp src/PluginProcessor.h
    src/PluginEditor.cpp    src/PluginEditor.h
    src/CoheraTypes.h

    # Parameters
    src/parameters/ParameterSet.h
    src/parameters/ParameterManager.h

    # Engine
    src/engine/SaturationEngine.h
    src/engine/TransientEngine.h
    src/engine/AnalogModelingEngine.h
    src/engine/BandProcessingEngine.h
    src/engine/FilterBankEngine.h
    src/engine/MixEngine.h
    src/engine/ProcessingEngine.h

    # Network
    src/network/NetworkController.h

    # Tests - DISABLED IN MAIN PLUGIN
    # src/tests/TestHelpers.h
    # src/tests/TestAudioGenerator.h
    # src/tests/EngineIntegrationTests.cpp
    # src/tests/RealWorldScenarios.cpp
    # src/tests/IndustryStandardTests.cpp
    # src/tests/TestRunner.cpp

    # DSP Module
    src/dsp/FilterBank.cpp  src/dsp/FilterBank.h
    src/dsp/Waveshaper.h
    src/dsp/MathSaturator.h
    src/dsp/DCBlocker.h
    src/dsp/Envelope.h
    src/dsp/DynamicsRestorer.h       # <-- NEW
    src/dsp/InteractionEngine.h      # <-- NEW
    src/dsp/MSMatrix.h               # <-- NEW
    src/dsp/PsychoAcousticGain.h     # <-- NEW
    src/dsp/SidechainNormalizer.h    # <-- NEW
    src/dsp/AutoGainStage.h          # <-- NEW
    src/dsp/TransientDetector.h      # <-- NEW
    src/dsp/TransientSplitter.h      # <-- NEW
    src/dsp/VoltageRegulator.h       # <-- NEW
    src/dsp/ThermalModel.h           # <-- NEW
    src/dsp/StereoVariance.h         # <-- NEW
    src/dsp/NoiseBreather.h          # <-- NEW
    src/dsp/DeltaMonitor.h           # <-- NEW
    src/dsp/StereoFocus.h            # <-- NEW
    src/dsp/HarmonicEntropy.h        # <-- NEW
    src/FIR/fir_coeffs_multi_sr.h
    src/FIR/fir_minphase_128.h
    src/CoheraTypes.h
    src/analyzer/AnalyzerPipelineConfig.h
    # src/dsp/Waveshaper.cpp (добавим позже)

    # Network Module
    src/network/NetworkManager.h    # <-- NEW
    # src/network/NetworkManager.cpp (добавим позже)

    # UI Module
    src/ui/CoheraLookAndFeel.h
    src/ui/components/SaturationCore.h
    src/ui/components/NetworkBrain.h
    src/ui/visuals/BioScanner.h
    src/ui/visuals/GlitchOverlay.h
    src/ui/visuals/TechDecor.h
    # src/ui/LookAndFeel.cpp (добавим позже)
)

# 4. Настройки компиляции (Максимальная строгость для чистоты!)
target_compile_features(Cohera_Saturator PRIVATE cxx_std_20)

target_compile_options(Cohera_Saturator PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4> # Windows warning level 4
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Wpedantic> # Linux/Mac warnings
)

# 5. Линковка с модулями JUCE
target_link_libraries(Cohera_Saturator PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
)

# Create lightweight test executable (only DSP logic, no UI)
add_executable(Cohera_Tests
    src/tests/TestRunner.cpp
    src/tests/TestHelpers.h
    src/tests/TestAudioGenerator.h
    src/tests/EngineIntegrationTests.cpp
    src/tests/RealWorldScenarios.cpp

    # Core DSP modules (no UI dependencies)
    src/dsp/FilterBank.cpp
    src/dsp/FilterBank.h
    src/dsp/MathSaturator.h
    src/dsp/DCBlocker.h
    src/dsp/Envelope.h
    src/dsp/TransientSplitter.h
    src/dsp/ThermalModel.h
    src/dsp/HarmonicEntropy.h
    src/dsp/StereoVariance.h
    src/dsp/MSMatrix.h

    # FIR coefficients
    src/FIR/fir_coeffs_multi_sr.h
    src/FIR/fir_minphase_128.h

    # Network and parameters
    src/network/NetworkManager.h
    src/parameters/ParameterSet.h
    src/parameters/ParameterManager.h

    # Engines
    src/engine/SaturationEngine.h
    src/engine/TransientEngine.h
    src/engine/AnalogModelingEngine.h
    src/engine/BandProcessingEngine.h
    src/engine/FilterBankEngine.h
    src/engine/MixEngine.h
    src/engine/ProcessingEngine.h

    # Type definitions
    src/CoheraTypes.h
)

target_include_directories(Cohera_Tests PRIVATE src)

target_link_libraries(Cohera_Tests PRIVATE
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_audio_processors
    juce::juce_dsp
)

set_target_properties(Cohera_Tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/Cohera_Saturator_artefacts/Tests"
)

# Basic Signal Flow Test Runner (Fundamental Sanity Check)
add_executable(BasicSignalFlowTest
    src/tests/SimpleTestRunner.cpp
    src/tests/TestHelpers.h
    src/tests/TestAudioGenerator.h
    src/tests/DAWSimulationTest.cpp
    src/tests/ComprehensiveParameterTests.cpp
    src/tests/DeadCodeHunter.cpp
    src/PluginProcessor.cpp
    src/PluginEditor.cpp
    src/CoheraTypes.h
    src/parameters/ParameterSet.h
    src/parameters/ParameterManager.h
    src/engine/SaturationEngine.h
    src/engine/TransientEngine.h
    src/engine/AnalogModelingEngine.h
    src/engine/BandProcessingEngine.h
    src/engine/FilterBankEngine.h
    src/engine/MixEngine.h
    src/engine/ProcessingEngine.h
    src/dsp/FilterBank.cpp
    src/dsp/FilterBank.h
    src/dsp/MathSaturator.h
    src/dsp/DCBlocker.h
    src/dsp/Envelope.h
    src/dsp/TransientSplitter.h
    src/dsp/ThermalModel.h
    src/dsp/HarmonicEntropy.h
    src/dsp/StereoVariance.h
    src/dsp/MSMatrix.h
    src/FIR/fir_coeffs_multi_sr.h
    src/FIR/fir_minphase_128.h
    src/network/NetworkManager.h
    src/ui/SimpleFFT.h
    src/ui/Components/SpectrumVisor.h
    src/ui/Components/TopBar.h
    src/ui/Components/SaturationCore.h
    src/ui/Components/NetworkBrain.h
    src/ui/CoheraLookAndFeel.h
    src/analyzer/AnalyzerPipelineConfig.h
)

target_include_directories(BasicSignalFlowTest PRIVATE src)

target_link_libraries(BasicSignalFlowTest PRIVATE
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_core
)

set_target_properties(BasicSignalFlowTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Simple standalone test runner (no complex dependencies)
add_executable(SimpleDSPTest
    src/tests/SimpleTestRunner.cpp
    src/tests/TestAudioGenerator.h
    src/dsp/MathSaturator.h
    src/dsp/DCBlocker.h
    src/CoheraTypes.h
)

target_include_directories(SimpleDSPTest PRIVATE src)

target_link_libraries(SimpleDSPTest PRIVATE
    juce::juce_core
    juce::juce_audio_basics
    juce::juce_gui_basics
)

set_target_properties(SimpleDSPTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)

# Industry Standard Test Runner
add_executable(IndustryStandardTest
    src/tests/IndustryStandardTestRunner.cpp
    src/tests/TestHelpers.h
    src/PluginProcessor.cpp src/PluginProcessor.h
    src/PluginEditor.cpp src/PluginEditor.h
    src/CoheraTypes.h
    src/parameters/ParameterSet.h
    src/parameters/ParameterManager.h
    src/engine/SaturationEngine.h
    src/engine/TransientEngine.h
    src/engine/AnalogModelingEngine.h
    src/engine/BandProcessingEngine.h
    src/engine/FilterBankEngine.h
    src/engine/MixEngine.h
    src/engine/ProcessingEngine.h
    src/dsp/FilterBank.cpp src/dsp/FilterBank.h
    src/dsp/MathSaturator.h
    src/dsp/DCBlocker.h
    src/dsp/Envelope.h
    src/dsp/TransientSplitter.h
    src/dsp/ThermalModel.h
    src/dsp/HarmonicEntropy.h
    src/dsp/StereoVariance.h
    src/dsp/MSMatrix.h
    src/FIR/fir_coeffs_multi_sr.h
    src/FIR/fir_minphase_128.h
    src/network/NetworkManager.h
    src/ui/SimpleFFT.h
    src/ui/Components/SpectrumVisor.h
    src/ui/Components/TopBar.h
    src/ui/Components/SaturationCore.h
    src/ui/Components/NetworkBrain.h
    src/ui/CoheraLookAndFeel.h
    src/analyzer/AnalyzerPipelineConfig.h
)

target_include_directories(IndustryStandardTest PRIVATE src)

target_link_libraries(IndustryStandardTest PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_core
    juce::juce_data_structures
    juce::juce_events
)

set_target_properties(IndustryStandardTest PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}"
)
