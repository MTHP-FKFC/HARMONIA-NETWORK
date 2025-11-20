#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <juce_core/juce_core.h>

using namespace Cohera;
#include "dsp/Waveshaper.h"
#include "dsp/InteractionEngine.h"
#include "dsp/MSMatrix.h"

// Include test files for in-plugin testing
#include "tests/TestHelpers.h"
#include "tests/TestAudioGenerator.h"
#include "tests/EngineIntegrationTests.cpp"
#include "tests/RealWorldScenarios.cpp"
#include "tests/IndustryStandardTests.cpp"


CoheraSaturatorAudioProcessor::CoheraSaturatorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     ),
#endif
       apvts(*this, nullptr, "PARAMETERS", createParameterLayout()),
       paramManager(apvts)
{
    // Запуск интеграционных тестов в Debug режиме
#if JUCE_DEBUG
    std::cout << "=== RUNNING COHERA SATURATOR INTEGRATION TESTS ===" << std::endl;
    juce::UnitTestRunner runner;
    runner.setPassesAreLogged(true); // Включаем вывод успешных тестов
    runner.runAllTests();
    std::cout << "=== TESTS COMPLETED ===" << std::endl;
#endif

    // Новая архитектура инициализирована в конструкторе
}

CoheraSaturatorAudioProcessor::~CoheraSaturatorAudioProcessor()
{
    // Гарантируем освобождение слота при удалении плагина
    if (myInstanceIndex != -1) {
        NetworkManager::getInstance().unregisterInstance(myInstanceIndex);
    }
}

// Настройка параметров (пока заглушки)
juce::AudioProcessorValueTreeState::ParameterLayout CoheraSaturatorAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // Drive: от 0% до 100% (внутри замапим это на 0..24 dB или больше)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "drive_master", "Drive",
        juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 0.0f));

    // Mix: 0% (Dry) .. 100% (Wet)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f)); // По умолчанию Wet

    // Output Gain: +/- 12 dB
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "output_gain", "Output",
        juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

    // Режим работы: 5 режимов взаимодействия
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "mode", "Interaction Mode",
        juce::StringArray{
            "Unmasking (Duck)",   // 0: Реф громкий -> Мы чище
            "Ghost (Follow)",     // 1: Реф громкий -> Мы злее
            "Gated (Reverse)",    // 2: Реф тихий -> Мы злее
            "Stereo Bloom",       // 3: Реф громкий -> Мы расширяемся
            "Sympathetic"         // 4: Реф частоты -> Мы резонируем
        }, 0));

    // Тип сатурации (базовый выбор пользователя)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "sat_type", "Saturation Type",
        juce::StringArray{
            "Warm Tube",    // 0: Tanh
            "Asymmetric",   // 1: Even Harmonics
            "Hard Clip",    // 2: Brickwall
            "Bit Crush"     // 3: Digital
        }, 0));

    // === EMPHASIS FILTERS (Tone Shaping) ===

    // Tighten (Pre HPF): 10 Hz (выкл) ... 1000 Hz
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "tone_tighten", "Tighten (Pre HPF)",
        juce::NormalisableRange<float>(10.0f, 1000.0f, 1.0f, 0.5f), 10.0f));

    // Smooth (Post LPF): 22000 Hz (выкл) ... 2000 Hz
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "tone_smooth", "Smooth (Post LPF)",
        juce::NormalisableRange<float>(2000.0f, 22000.0f, 1.0f, 0.5f), 22000.0f));

    // Dynamics Preservation
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "dynamics", "Dynamics Preservation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));

    // === NETWORK CONTROL (The Holy Trinity) ===

    // 1. Depth: Насколько сильно мы реагируем (0% = игнорируем сеть, 100% = полный морфинг)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "net_depth", "Interaction Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f)); // Default 100%

    // 2. Smooth: Время реакции (0ms = мгновенная, 200ms = ленивая)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "net_smooth", "Reaction Smooth",
        juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f), 10.0f));  // Default 10ms

    // 3. Sensitivity: Чувствительность (0% = не реагируем, 200% = гипер-чувствительный)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "net_sens", "Sensitivity",
        juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f), 100.0f)); // Default 100%

    // === GLOBAL HEAT ===
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "heat_amount", "Global Heat",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f)); // Default 0% (выкл)

    // === PUNCH (Transient Control) ===
    // Punch: -100% (Dirty Attack) ... 0% (Off) ... +100% (Clean Attack)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "punch", "Punch",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f));

    // === ANALOG MODELING ===
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "analog_drift", "Analog Drift",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f));

    // === MOJO (Analog Imperfections) ===
    // Variance: 0% (Perfect Digital) ... 100% (Broken Analog)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "variance", "Stereo Variance",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f));

    // Noise: 0% (Silence) ... 100% (Vintage Tape)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "noise", "Noise Floor",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f));

    // === PROFESSIONAL TOOLS ===
    // Focus: -100 (Mid Only) ... 0 (Stereo) ... +100 (Side Only)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "focus", "Stereo Focus",
        juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f), 0.0f));

    // Delta: Переключатель (On/Off)
    layout.add(std::make_unique<juce::AudioParameterBool>(
        "delta", "Delta Listen", false));

    // === HARMONIC ENTROPY ===
    // Entropy: 0% (Digital) ... 100% (Broken Analog)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "entropy", "Harmonic Entropy",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f));

    // === DIVINE MATH MODE ===
    // Алгоритм на базе фундаментальных констант Вселенной
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "math_mode", "Algo (Divine)",
        juce::StringArray{
            "Golden Ratio (Harmony)",
            "Euler Tube (Warmth)",
            "Pi Fold (Width)",
            "Fibonacci (Grit)",
            "Super Ellipse (Punch)"
        }, 0));

    // === QUALITY MODE ===
    // Eco = без оверсемплинга, Pro = 4x с линейной фазой
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "quality", "Quality",
        juce::StringArray{"Eco (Low CPU)", "Pro (High Quality)"}, 1)); // Default Pro

    // Group & Role
    layout.add(std::make_unique<juce::AudioParameterInt>("group_id", "Group ID", 0, 7, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("role", "Role", juce::StringArray{"Listener", "Reference"}, 0));

    return layout;
}

void CoheraSaturatorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Новая архитектура - просто делегируем
    juce::dsp::ProcessSpec spec { sampleRate, (uint32)samplesPerBlock, 2 };
    processingEngine.prepare(spec);
    analyzer.prepare();

    setLatencySamples((int)processingEngine.getLatency());
}

void CoheraSaturatorAudioProcessor::releaseResources()
{
    // Новая архитектура - делегируем
    processingEngine.reset();
}

void CoheraSaturatorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Исходные размеры
    const int originalNumSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), 2);

    // Новая архитектура - получаем параметры и обрабатываем
    auto params = paramManager.getCurrentParams();

    // Делаем копию для Dry сигнала (критично для MixEngine)
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // Обрабатываем через ProcessingEngine
    processingEngine.processBlockWithDry(buffer, dryBuffer, params);

    // Кормим анализатор выходным сигналом
    for (int i = 0; i < originalNumSamples; ++i)
    {
        float monoOut = (buffer.getSample(0, i) + (numCh > 1 ? buffer.getSample(1, i) : buffer.getSample(0, i))) * 0.5f;
        analyzer.pushSample(monoOut);
    }
}

// === Сохранение состояния ===

void CoheraSaturatorAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml(state.createXml());
    copyXmlToBinary(*xml, destData);
}

void CoheraSaturatorAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState(getXmlFromBinary(data, sizeInBytes));
    if (xmlState.get() != nullptr)
        if (xmlState->hasTagName(apvts.state.getType()))
            apvts.replaceState(juce::ValueTree::fromXml(*xmlState));
}

juce::AudioProcessorEditor* CoheraSaturatorAudioProcessor::createEditor()
{
    return new CoheraSaturatorAudioProcessorEditor(*this);
}

