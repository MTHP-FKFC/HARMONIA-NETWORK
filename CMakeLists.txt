cmake_minimum_required(VERSION 3.15)

project(Cohera_Saturator VERSION 0.1.0)

# ==============================================================================
# 1. JUCE Configuration
# ==============================================================================
set(JUCE_DIR "" CACHE PATH "Path to JUCE source tree")

if (JUCE_DIR)
    message(STATUS "Using JUCE from: ${JUCE_DIR}")
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    # Fallback: Try default location
    set(DEFAULT_JUCE_PATH "/Users/macos/JUCE")
    if (EXISTS "${DEFAULT_JUCE_PATH}")
        message(STATUS "JUCE_DIR not set, using default: ${DEFAULT_JUCE_PATH}")
        add_subdirectory(${DEFAULT_JUCE_PATH} JUCE)
    else()
        message(FATAL_ERROR "JUCE not found. Please set JUCE_DIR cache variable.")
    endif()
endif()

# Use legacy parameter IDs to avoid VST2/VST3 conflicts
add_compile_definitions(JUCE_FORCE_USE_LEGACY_PARAM_IDS=1)

# Option to build tests
option(COHERA_BUILD_TESTS "Build unit/regression test executables" ON)

# Set C++ Standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ==============================================================================
# 2. cohera_core Library (DSP, Engine, Network, Parameters)
# ==============================================================================
set(COHERA_CORE_SOURCES
    # DSP
    src/dsp/FilterBank.cpp
    src/dsp/FilterBank.h
    src/dsp/MathSaturator.h
    src/dsp/DCBlocker.h
    src/dsp/Envelope.h
    src/dsp/DynamicsRestorer.h
    src/dsp/InteractionEngine.h
    src/dsp/MSMatrix.h
    src/dsp/PsychoAcousticGain.h
    src/dsp/SidechainNormalizer.h
    src/dsp/AutoGainStage.h
    src/dsp/TransientDetector.h
    src/dsp/TransientSplitter.h
    src/dsp/VoltageRegulator.h
    src/dsp/ThermalModel.h
    src/dsp/StereoVariance.h
    src/dsp/NoiseBreather.h

    src/dsp/StereoFocus.h
    src/dsp/HarmonicEntropy.h
    src/dsp/Waveshaper.h

    # FIR
    src/FIR/fir_coeffs_multi_sr.h
    src/FIR/fir_minphase_128.h

    # Parameters
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

    # Network
    src/network/NetworkManager.h
    src/network/NetworkController.h
    
    # Utils
    src/utils/TrackAudioFifo.h
    src/analyzer/AnalyzerPipelineConfig.h

    # Common Types
    src/CoheraTypes.h
)

add_library(cohera_core STATIC ${COHERA_CORE_SOURCES})

target_include_directories(cohera_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(cohera_core PUBLIC
    juce::juce_core
    juce::juce_dsp
    juce::juce_audio_basics
)

# Compiler warnings for core
if (MSVC)
    target_compile_options(cohera_core PRIVATE /W4)
else()
    target_compile_options(cohera_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ==============================================================================
# 3. Cohera_Saturator Plugin
# ==============================================================================
juce_add_plugin(Cohera_Saturator
    COMPANY_NAME "CoheraAudio"
    BUNDLE_ID "com.coheraaudio.coherasaturator"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Cohera Saturator"
    plugin_name "Cohera Saturator"
    AU_MAIN_TYPE kAudioUnitType_Effect
    PLUGIN_MANUFACTURER_CODE Cohr
    PLUGIN_CODE Csat
)

target_sources(Cohera_Saturator PRIVATE
    src/Main.cpp
    src/PluginProcessor.cpp src/PluginProcessor.h
    src/PluginEditor.cpp    src/PluginEditor.h
    
    # UI Components
    src/ui/CoheraLookAndFeel.h

    src/ui/components/NetworkBrain.h
    src/ui/visuals/BioScanner.h
    src/ui/visuals/GlitchOverlay.h
    src/ui/visuals/TechDecor.h
    src/ui/SimpleFFT.h
    src/ui/Components/SpectrumVisor.h

)

target_link_libraries(Cohera_Saturator PRIVATE
    cohera_core
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_gui_basics
    juce::juce_gui_extra
    juce::juce_graphics
    juce::juce_data_structures
    juce::juce_events
)

target_compile_features(Cohera_Saturator PRIVATE cxx_std_20)

# ==============================================================================
# 4. Tests
# ==============================================================================
if (COHERA_BUILD_TESTS)
    message(STATUS "Configuring Test Suites...")

    # --- Unit Tests ---
    add_executable(Cohera_Tests
        src/tests/TestRunner.cpp
        src/tests/TestHelpers.h
        src/tests/TestAudioGenerator.h
        src/tests/EngineIntegrationTests.cpp
        src/tests/RealWorldScenarios.cpp
    )

    target_link_libraries(Cohera_Tests PRIVATE
        cohera_core
        juce::juce_core
        juce::juce_audio_basics
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_recommended_config_flags
    )
    
    set_target_properties(Cohera_Tests PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    # --- Stress Test ---
    add_executable(StressTest
        tests/stress_test.cpp
        src/PluginProcessor.cpp src/PluginProcessor.h
        src/PluginEditor.cpp src/PluginEditor.h
        # UI sources needed for PluginEditor
        src/ui/CoheraLookAndFeel.h
    
        src/ui/components/NetworkBrain.h
        src/ui/visuals/BioScanner.h
        src/ui/visuals/GlitchOverlay.h
        src/ui/visuals/TechDecor.h
        src/ui/SimpleFFT.h
        src/ui/Components/SpectrumVisor.h
    
    )

    target_include_directories(StressTest PRIVATE src)

    target_link_libraries(StressTest PRIVATE
        cohera_core
        juce::juce_audio_processors
        juce::juce_dsp
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_gui_basics
        juce::juce_gui_extra
        juce::juce_graphics
    )
    
    set_target_properties(StressTest PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests"
    )

    # --- Regression Tests ---
    
    # Generate Test Signals
    add_executable(GenerateTestSignals
        tests/regression/generate_test_signals.cpp
        tests/regression/SignalGenerator.h
    )
    target_include_directories(GenerateTestSignals PRIVATE tests/regression)
    target_link_libraries(GenerateTestSignals PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_core)
    set_target_properties(GenerateTestSignals PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

    # Generate Instrument Signals
    add_executable(GenerateInstrumentSignals
        tests/regression/generate_instrument_signals.cpp
        tests/regression/SignalGenerator.h
    )
    target_include_directories(GenerateInstrumentSignals PRIVATE tests/regression)
    target_link_libraries(GenerateInstrumentSignals PRIVATE juce::juce_audio_utils juce::juce_audio_formats juce::juce_core)
    set_target_properties(GenerateInstrumentSignals PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

    # Process Test Signals (Headless Plugin)
    add_executable(ProcessTestSignals
        tests/regression/process_test_signals.cpp
        tests/regression/SignalGenerator.h
        src/PluginProcessor.cpp src/PluginProcessor.h
        src/PluginEditor.cpp src/PluginEditor.h
        # UI sources needed for PluginEditor
        src/ui/CoheraLookAndFeel.h
    
        src/ui/components/NetworkBrain.h
        src/ui/visuals/BioScanner.h
        src/ui/visuals/GlitchOverlay.h
        src/ui/visuals/TechDecor.h
        src/ui/SimpleFFT.h
        src/ui/Components/SpectrumVisor.h
    
    )
    target_include_directories(ProcessTestSignals PRIVATE src tests/regression)
    target_link_libraries(ProcessTestSignals PRIVATE
        cohera_core
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    )
    set_target_properties(ProcessTestSignals PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

    # Test Audio Regression Runner
    add_executable(TestAudioRegression
        tests/regression/test_audio_regression.cpp
        tests/regression/SignalGenerator.h
        tests/regression/AudioComparator.h
        src/PluginProcessor.cpp src/PluginProcessor.h
        src/PluginEditor.cpp src/PluginEditor.h
        # UI sources needed for PluginEditor
        src/ui/CoheraLookAndFeel.h
    
        src/ui/components/NetworkBrain.h
        src/ui/visuals/BioScanner.h
        src/ui/visuals/GlitchOverlay.h
        src/ui/visuals/TechDecor.h
        src/ui/SimpleFFT.h
        src/ui/Components/SpectrumVisor.h
    
    )
    target_include_directories(TestAudioRegression PRIVATE src tests/regression)
    target_link_libraries(TestAudioRegression PRIVATE
        cohera_core
        juce::juce_audio_utils
        juce::juce_audio_processors
        juce::juce_audio_formats
        juce::juce_dsp
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
    )
    set_target_properties(TestAudioRegression PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tests")

endif()
