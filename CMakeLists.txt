cmake_minimum_required(VERSION 3.15)

project(Cohera_Saturator VERSION 0.1.0)

# 1. Подключаем JUCE (предполагаем, что он лежит рядом или установлен глобально)
# Если используешь подмодуль git: add_subdirectory(JUCE)
add_subdirectory(/Users/macos/JUCE JUCE)

# Используем legacy parameter IDs чтобы избежать конфликтов VST2/VST3
add_compile_definitions(JUCE_FORCE_USE_LEGACY_PARAM_IDS=1)

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
    src/engine/BandProcessingEngineNew.h

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
