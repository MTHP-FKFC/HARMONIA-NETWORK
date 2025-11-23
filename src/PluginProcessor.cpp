#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <juce_core/juce_core.h>

using namespace Cohera;
#include "dsp/InteractionEngine.h"
#include "dsp/MSMatrix.h"
#include "dsp/Waveshaper.h"

// Include test files for in-plugin testing
// (Tests are now built as separate executables)

CoheraSaturatorAudioProcessor::CoheraSaturatorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(
          BusesProperties()
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
#endif
      apvts(*this, nullptr, "PARAMETERS", createParameterLayout()),
      paramManager(apvts),
      processingEngine() {

  // Новая архитектура инициализирована в конструкторе
  // ProcessingEngine работает с NetworkManager через внутренний NetworkController
}

CoheraSaturatorAudioProcessor::~CoheraSaturatorAudioProcessor() {
  // Гарантируем освобождение слота при удалении плагина
  if (myInstanceIndex != -1) {
    NetworkManager::getInstance().unregisterInstance(myInstanceIndex);
  }
}

// Настройка параметров (пока заглушки)
juce::AudioProcessorValueTreeState::ParameterLayout
CoheraSaturatorAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;

  // Drive: от 0% до 100% (внутри замапим это на 0..24 dB или больше)
  // Golden Init: 20% чтобы пользователь сразу услышал теплоту
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "drive_master", "Drive",
      juce::NormalisableRange<float>(0.0f, 100.0f, 0.1f), 20.0f));

  // Mix: 0% (Dry) .. 100% (Wet)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "mix", "Mix", juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
      100.0f)); // По умолчанию Wet

  // Output Gain: +/- 12 dB
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "output_gain", "Output",
      juce::NormalisableRange<float>(-12.0f, 12.0f, 0.1f), 0.0f));

  // Режим работы: 5 режимов взаимодействия
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      "mode", "Interaction Mode",
      juce::StringArray{
          "Unmasking (Duck)", // 0: Освободи место
          "Ghost (Follow)",   // 1: Синхронная энергия
          "Gated (Reverse)",  // 2: Играй в паузах
          "Stereo Bloom",     // 3: Пространственный взрыв
          "Sympathetic",      // 4: Резонанс
          "Transient Clone",  // 5: Заимствование атаки
          "Spectral Sculpt",  // 6: Динамический EQ
          "Voltage Starve",   // 7: Энергетический вампиризм
          "Entropy Storm",    // 8: Управляемый хаос
          "Harmonic Shield"   // 9: Анти-сатурация
      },
      0));

  // === CASCADE (Output Stage) ===
  layout.add(std::make_unique<juce::AudioParameterBool>(
      "cascade", "Cascade (Output Limiter)", false));

  // === NETWORK REACTION ===
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      "net_reaction", "Network Reaction",
      juce::StringArray{
          "Clean Gain",  // 0: Просто громкость
          "Drive Boost", // 1: Разгон алгоритма
          "Rectify",     // 2: Гармоники (Ghost)
          "Bit Crush"    // 3: Глитч (Digital)
      },
      1)); // Default Drive Boost

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

  // 1. Depth: Насколько сильно мы реагируем (0% = игнорируем сеть, 100% =
  // полный морфинг)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "net_depth", "Interaction Depth",
      juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
      100.0f)); // Default 100%

  // 2. Smooth: Время реакции (0ms = мгновенная, 200ms = ленивая)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "net_smooth", "Reaction Smooth",
      juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f),
      10.0f)); // Default 10ms

  // 3. Sensitivity: Чувствительность (0% = не реагируем, 200% =
  // гипер-чувствительный)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "net_sens", "Sensitivity",
      juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f),
      100.0f)); // Default 100%

  // === GLOBAL HEAT ===
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "heat_amount", "Global Heat",
      juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f),
      0.0f)); // Default 0% (выкл)

  // === PUNCH (Transient Control) ===
  // Punch: -100% (Dirty Attack) ... 0% (Off) ... +100% (Clean Attack)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "punch", "Punch", juce::NormalisableRange<float>(-100.0f, 100.0f, 1.0f),
      0.0f));

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
  layout.add(std::make_unique<juce::AudioParameterBool>("delta", "Delta Listen",
                                                        false));

  // === HARMONIC ENTROPY ===
  // Entropy: 0% (Digital) ... 100% (Broken Analog)
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      "entropy", "Harmonic Entropy",
      juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 0.0f));

  // === UNIFIED SATURATION MODE ===
  // Алгоритм на базе фундаментальных констант Вселенной + классика
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      "math_mode", "Algorithm",
      juce::StringArray{
          // === DIVINE SERIES ===
          "Golden Ratio", "Euler Tube", "Pi Fold", "Fibonacci", "Super Ellipse",
          // === COSMIC PHYSICS ===
          "Lorentz Force", "Riemann Zeta", "Mandelbrot Set", "Quantum Well",
          "Planck Limit",
          // === CLASSIC SERIES ===
          "Analog Tape", "Vintage Console", "Diode Class A", "Tube Driver",
          "Digital Fuzz", "Bit Decimator", "Rectifier"},
      0));

  // === QUALITY MODE ===
  // Eco = без оверсемплинга, Pro = 4x с линейной фазой
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      "quality", "Quality",
      juce::StringArray{"Eco (Low CPU)", "Pro (High Quality)"},
      1)); // Default Pro

  // Group & Role
  layout.add(std::make_unique<juce::AudioParameterInt>("group_id", "Group ID",
                                                       0, 7, 0));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      "role", "Role", juce::StringArray{"Listener", "Reference"}, 0));

  return layout;
}

void CoheraSaturatorAudioProcessor::prepareToPlay(double sampleRate,
                                                  int samplesPerBlock) {
  // 1. Создаем спецификацию
  juce::dsp::ProcessSpec spec{sampleRate, (juce::uint32)samplesPerBlock, 2};

  // 2. Полная переинициализация движка
  // Это должно:
  // - Пересчитать все коэффициенты фильтров (TPT, IIR)
  // - Очистить линии задержки (DelayLines)
  // - Сбросить огибающие (Envelopes)
  processingEngine.prepare(spec);

  // 3. Сброс анализатора (чтобы старый спектр не висел)
  analyzer.prepare();
  analyzer.setSampleRate((float)sampleRate);

  // 4. CRITICAL FIX: Pre-allocate buffers to avoid heap allocations in processBlock
  // Allocate with 2x safety margin for variable block sizes
  dryBuffer.setSize(2, samplesPerBlock * 2, false, true, false);
  monoBuffer.setSize(1, samplesPerBlock * 2, false, true, false);

  // 5. Сообщаем хосту новую задержку (целое значение, как требует JUCE)
  const int hostLatency = juce::roundToInt(processingEngine.getLatency());
  setLatencySamples(hostLatency);
  juce::Logger::writeToLog("Host latency updated: " +
                           juce::String(hostLatency));

  // 6. Очистка "хвостов" (опционально, но полезно при смене треков)
  processingEngine.reset();
}

void CoheraSaturatorAudioProcessor::releaseResources() {
  // Новая архитектура - делегируем
  processingEngine.reset();
}

void CoheraSaturatorAudioProcessor::processBlock(
    juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages) {
  juce::ScopedNoDenormals noDenormals;

  // Try to lock process. If we can't, it means state is loading.
  // Mute output and return to avoid race conditions.
  juce::GenericScopedTryLock<juce::CriticalSection> lock(processLock);
  if (!lock.isLocked()) {
    buffer.clear();
    return;
  }

  // Исходные размеры
  const int originalNumSamples = buffer.getNumSamples();
  const int numCh = juce::jmin(buffer.getNumChannels(), 2);

  // Новая архитектура - получаем параметры и обрабатываем
  auto params = paramManager.getCurrentParams();

  // CRITICAL FIX: Reuse pre-allocated buffer (no heap allocation!)
  // Safety check: ensure buffer is large enough
  if (dryBuffer.getNumSamples() < originalNumSamples) {
    juce::Logger::writeToLog("WARNING: Block size exceeded prepared size!");
    dryBuffer.setSize(2, originalNumSamples, false, false, true);
  }
  
  // Copy data into pre-allocated buffer
  for (int ch = 0; ch < numCh; ++ch) {
    dryBuffer.copyFrom(ch, 0, buffer, ch, 0, originalNumSamples);
  }

  // Обрабатываем через ProcessingEngine
  processingEngine.processBlockWithDry(buffer, dryBuffer, params);

  // Собираем данные для NebulaShaper (каждый N-й сэмпл для производительности)
  static int sampleCounter = 0;
  if (++sampleCounter % 64 == 0) { // Каждые 64 сэмпла
    for (int i = 0; i < originalNumSamples && i < 10;
         ++i) { // Максимум 10 точек за блок
      float in = dryBuffer.getSample(0, i);
      float out = buffer.getSample(0, i);
      // Нормализуем к -1..1 для визуализации
      in = juce::jlimit(-1.0f, 1.0f, in);
      out = juce::jlimit(-1.0f, 1.0f, out);
      pushVisualizerData(in, out);
    }
  }

  // CRITICAL FIX: Reuse pre-allocated monoBuffer (no heap allocation!)
  // Safety check: ensure buffer is large enough
  if (monoBuffer.getNumSamples() < originalNumSamples) {
    juce::Logger::writeToLog("WARNING: Mono buffer size exceeded prepared size!");
    monoBuffer.setSize(1, originalNumSamples, false, false, true);
  }
  
  // Кормим анализатор выходным сигналом (блочно и безопасно)
  if (numCh > 1) {
    monoBuffer.copyFrom(0, 0, buffer, 0, 0, originalNumSamples);
    monoBuffer.addFrom(0, 0, buffer, 1, 0, originalNumSamples);
    monoBuffer.applyGain(0.5f);
  } else {
    monoBuffer.copyFrom(0, 0, buffer, 0, 0, originalNumSamples);
  }
  analyzer.pushBlock(monoBuffer);

  // Измеряем output RMS для визуализации
  float outputRms = 0.0f;
  const int numChannels = buffer.getNumChannels();
  if (numChannels > 0) {
    for (int ch = 0; ch < numChannels; ++ch)
      outputRms += buffer.getRMSLevel(ch, 0, buffer.getNumSamples());
    outputRms /= numChannels;
  }
  outputRMS.store(outputRms);
  
  // Обновляем термальное состояние для BioScanner
  float realThermalTemp = processingEngine.getAverageTemperature();
  currentThermalState.store(realThermalTemp);
}

// === Сохранение состояния ===

void CoheraSaturatorAudioProcessor::getStateInformation(
    juce::MemoryBlock &destData) {
  auto state = apvts.copyState();
  std::unique_ptr<juce::XmlElement> xml(state.createXml());
  copyXmlToBinary(*xml, destData);
}

void CoheraSaturatorAudioProcessor::setStateInformation(const void *data,
                                                        int sizeInBytes) {
  // Lock processing to prevent race conditions during state load
  const juce::ScopedLock lock(processLock);

  std::unique_ptr<juce::XmlElement> xmlState(
      getXmlFromBinary(data, sizeInBytes));
  if (xmlState.get() != nullptr) {
    if (xmlState->hasTagName(apvts.state.getType())) {
      apvts.replaceState(juce::ValueTree::fromXml(*xmlState));

      // Reset DSP engine to prevent audio glitches with new parameters
      processingEngine.reset();
    }
  }
}

juce::AudioProcessorEditor *CoheraSaturatorAudioProcessor::createEditor() {
#ifndef COHERA_HEADLESS
  return new CoheraSaturatorAudioProcessorEditor(*this);
#else
  return nullptr;
#endif
}
