#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/Waveshaper.h"
#include "dsp/InteractionEngine.h"
#include "dsp/MSMatrix.h"


CoheraSaturatorAudioProcessor::CoheraSaturatorAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     ),
#endif
       apvts(*this, nullptr, "PARAMETERS", createParameterLayout())
{
    // Инициализируем кроссовер
    filterBank = std::make_unique<PlaybackFilterBank>();
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

    // Group & Role
    layout.add(std::make_unique<juce::AudioParameterInt>("group_id", "Group ID", 0, 7, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("role", "Role", juce::StringArray{"Listener", "Reference"}, 0));

    return layout;
}

void CoheraSaturatorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // 1. Инициализируем Оверсемплер (4x)
    // 2 = 4x upsampling (2^2)
    // filterHalfBandFIREquiripple = Линейная фаза (высшее качество)
    // true = isMaxQuality (максимальное подавление зеркальных частот)
    oversampler = std::make_unique<juce::dsp::Oversampling<float>>(2, 2, juce::dsp::Oversampling<float>::filterHalfBandFIREquiripple, true);
    oversampler->initProcessing(samplesPerBlock);

    // Внутренняя частота (High Rate)
    double osSampleRate = sampleRate * (double)oversamplingFactor;
    // Внутренний размер блока (максимальный)
    int osBlockSize = samplesPerBlock * (int)oversamplingFactor;

    // 2. Готовим Фильтры (на ВЫСОКОЙ частоте!)
    FilterBankConfig config;
    config.sampleRate = osSampleRate; // <--- ВАЖНО!
    config.maxBlockSize = (juce::uint32)osBlockSize;
    config.numBands = kNumBands;
    config.phaseMode = FilterPhaseMode::LinearFIR256;
    config.profile = CrossoverProfile::Default;

    filterBank->prepare(config);

    // 3. Рассчитываем Латенси (Задержку)
    // Задержка Оверсемплера (в сэмплах исходной частоты)
    float osLatency = oversampler->getLatencyInSamples();

    // Задержка Кроссовера (она в сэмплах ВЫСОКОЙ частоты)
    // Переводим в сэмплы ОБЫЧНОЙ частоты (разделить на фактор)
    float xoverLatencyHigh = (float)filterBank->getLatencySamples();
    float xoverLatencyNorm = xoverLatencyHigh / (float)oversamplingFactor;

    // Общая задержка системы
    float totalLatency = osLatency + xoverLatencyNorm;

    setLatencySamples((int)totalLatency);

    // Настраиваем Dry Delay Line (на ОБЫЧНОЙ частоте)
    // Dry сигнал не идет в оверсемплер, он ждет снаружи
    dryDelayLine.prepare({ sampleRate, (juce::uint32)samplesPerBlock, 2 });
    dryDelayLine.setDelay(totalLatency);

    // === EMPHASIS FILTERS SETUP ===
    // Настраиваем фильтры (работают на ВЫСОКОЙ частоте)
    juce::dsp::ProcessSpec highSpec;
    highSpec.sampleRate = osSampleRate;
    highSpec.maximumBlockSize = (juce::uint32)osBlockSize;
    highSpec.numChannels = 1; // Обрабатываем каналы по отдельности

    for (int ch = 0; ch < 2; ++ch) {
        preFilters[ch].prepare(highSpec);
        preFilters[ch].setType(juce::dsp::StateVariableTPTFilterType::highpass);

        postFilters[ch].prepare(highSpec);
        postFilters[ch].setType(juce::dsp::StateVariableTPTFilterType::lowpass);
    }

    // Сглаживатели частот (на High Rate)
    smoothedTightenFreq.reset(osSampleRate, 0.05);
    smoothedTightenFreq.setCurrentAndTargetValue(10.0f);

    smoothedSmoothFreq.reset(osSampleRate, 0.05);
    smoothedSmoothFreq.setCurrentAndTargetValue(22000.0f);

    // 4. Буферы полос (увеличенного размера)
    bandBufferPtrs.resize(kNumBands);
    for (int i = 0; i < kNumBands; ++i) {
        bandBuffers[i].setSize(2, osBlockSize); // <--- Большой размер!
        bandBufferPtrs[i] = &bandBuffers[i];
    }

    // 5. Сглаживатели
    // High Rate сглаживатели (работают в цикле оверсемплинга)
    smoothedDrive.reset(osSampleRate, 0.05);
    smoothedSatBlend.reset(osSampleRate, 0.05);
    smoothedNetDepth.reset(osSampleRate, 0.05);
    smoothedNetSens.reset(osSampleRate, 0.05);

    // Low Rate сглаживатели (работают на финальном миксе)
    smoothedMix.reset(sampleRate, 0.05);
    smoothedOutput.reset(sampleRate, 0.05);
    smoothedDynamics.reset(sampleRate, 0.05);

    // Устанавливаем начальные значения
    smoothedDrive.setCurrentAndTargetValue(1.0f);
    smoothedOutput.setCurrentAndTargetValue(1.0f);
    smoothedMix.setCurrentAndTargetValue(1.0f);
    smoothedSatBlend.setCurrentAndTargetValue(0.0f);
    smoothedDynamics.setCurrentAndTargetValue(0.5f);
    smoothedNetDepth.setCurrentAndTargetValue(1.0f);
    smoothedNetSens.setCurrentAndTargetValue(1.0f);

    // 6. Сеть и детекторы (на обычной частоте - анализируем вход до апсемплинга)
    smoothedNetworkSignal.reset(sampleRate, 0.02);
    for (int i = 0; i < kNumBands; ++i) {
        smoothedNetworkBands[i].reset(sampleRate, 0.02);
        smoothedNetworkBands[i].setCurrentAndTargetValue(0.0f);
        bandEnvelopes[i].reset(sampleRate);
    }

    // Инициализация Dynamics Restorers
    for (int b = 0; b < kNumBands; ++b) {
        for (int ch = 0; ch < 2; ++ch) {
            dynamicsRestorers[b][ch].prepare(sampleRate);
        }
    }

    psychoGain.prepare(sampleRate);

    // 7. Плавный пуск (на обычной частоте)
    startupFader.reset(sampleRate, 0.2f);
    startupFader.setCurrentAndTargetValue(0.0f);
    startupFader.setTargetValue(1.0f);

    // 8. Сброс состояния One-Pole фильтра
    netSmoothState = 0.0f;

    // 9. Регистрация в сети для Global Heat
    if (myInstanceIndex == -1) {
        myInstanceIndex = NetworkManager::getInstance().registerInstance();
    }

    // Настройка сглаживателя Global Heat (тепловая инерция 200мс)
    smoothedGlobalHeat.reset(sampleRate, 0.2f);
    smoothedGlobalHeat.setCurrentAndTargetValue(0.0f);

    // === PUNCH INITIALIZATION ===
    // Инициализируем на ВЫСОКОЙ частоте (работаем внутри оверсемплинга)
    for(auto& bandSplitters : splitters)
        for(auto& splitter : bandSplitters)
            splitter.prepare(osSampleRate);

    smoothedPunch.reset(osSampleRate, 0.05);
    smoothedPunch.setCurrentAndTargetValue(0.0f);

    // === ANALOG MODELING INITIALIZATION ===
    psu.prepare(sampleRate); // PSU работает на обычной частоте

    for(int b = 0; b < kNumBands; ++b) {
        for(int ch = 0; ch < 2; ++ch) {
            tubes[b][ch].prepare(osSampleRate); // Лампы на высокой частоте
        }
    }

    smoothedAnalogDrift.reset(osSampleRate, 0.05);
    smoothedAnalogDrift.setCurrentAndTargetValue(0.0f);

    // === MOJO INITIALIZATION ===
    stereoDrift.prepare(sampleRate); // LFO на обычном SR
    noiseFloor.prepare(sampleRate);  // Шум на обычном SR

    smoothedVariance.reset(sampleRate, 0.05);
    smoothedNoise.reset(sampleRate, 0.05);
    smoothedVariance.setCurrentAndTargetValue(0.0f);
    smoothedNoise.setCurrentAndTargetValue(0.0f);

    // === PROFESSIONAL TOOLS ===
    smoothedFocus.reset(sampleRate, 0.05);
    smoothedFocus.setCurrentAndTargetValue(0.0f);

    // === HARMONIC ENTROPY ===
    for (int b = 0; b < kNumBands; ++b) {
        for (int ch = 0; ch < 2; ++ch) {
            entropyModules[b][ch].prepare(osSampleRate);
        }
    }

    smoothedEntropy.reset(sampleRate, 0.05);
    smoothedEntropy.setCurrentAndTargetValue(0.0f);
}

void CoheraSaturatorAudioProcessor::releaseResources()
{
    // Освобождаем слот в сети
    if (myInstanceIndex != -1) {
        NetworkManager::getInstance().unregisterInstance(myInstanceIndex);
        myInstanceIndex = -1;
    }

    // Освобождаем ресурсы фильтров
    filterBank->reset();
}

void CoheraSaturatorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Исходные размеры
    const int originalNumSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), 2);

    // --- 1. ПАРАМЕТРЫ ---

    float driveParam = *apvts.getRawParameterValue("drive_master"); // 0..100
    float mixParam   = *apvts.getRawParameterValue("mix");          // 0..100
    float outDb      = *apvts.getRawParameterValue("output_gain");

    // Умный маппинг драйва
    float targetSatBlend = juce::jlimit(0.0f, 1.0f, driveParam / 15.0f);
    float driveRest = juce::jmax(0.0f, driveParam - 15.0f);
    float targetDrive = 1.0f + (driveRest * 0.2f);

    // Читаем параметр Dynamics
    float dynParam = *apvts.getRawParameterValue("dynamics");

    // === GLOBAL HEAT ===
    float heatParam = *apvts.getRawParameterValue("heat_amount");

    // === PUNCH ===
    float punchParam = *apvts.getRawParameterValue("punch") / 100.0f; // -1.0 .. 1.0

    // === ANALOG DRIFT ===
    float analogParam = *apvts.getRawParameterValue("analog_drift");

    // === MOJO ===
    float varianceParam = *apvts.getRawParameterValue("variance");
    float noiseParam = *apvts.getRawParameterValue("noise");

    // === PROFESSIONAL TOOLS ===
    float focusParam = *apvts.getRawParameterValue("focus");
    bool deltaMode = *apvts.getRawParameterValue("delta") > 0.5f;

    // === HARMONIC ENTROPY ===
    float entropyParam = *apvts.getRawParameterValue("entropy");

    // === DIVINE MATH MODE ===
    int mathModeIdx = *apvts.getRawParameterValue("math_mode");
    MathMode currentMathMode = static_cast<MathMode>(mathModeIdx);

    // === EMPHASIS FILTERS ===
    float tightenParam = *apvts.getRawParameterValue("tone_tighten");
    float smoothParam  = *apvts.getRawParameterValue("tone_smooth");

    // === NETWORK CONTROL (The Holy Trinity) ===
    float pDepth = *apvts.getRawParameterValue("net_depth");
    float pSmooth = *apvts.getRawParameterValue("net_smooth");
    float pSens  = *apvts.getRawParameterValue("net_sens");

    // Сглаживаем параметры
    smoothedDrive.setTargetValue(targetDrive);
    smoothedSatBlend.setTargetValue(targetSatBlend);
    smoothedMix.setTargetValue(mixParam / 100.0f);
    smoothedOutput.setTargetValue(juce::Decibels::decibelsToGain(outDb));
    smoothedDynamics.setTargetValue(dynParam / 100.0f);

    // Filter frequency smoothing
    smoothedTightenFreq.setTargetValue(tightenParam);
    smoothedSmoothFreq.setTargetValue(smoothParam);

    // Punch parameter smoothing
    smoothedPunch.setTargetValue(punchParam);

    // Analog drift parameter smoothing
    smoothedAnalogDrift.setTargetValue(analogParam / 100.0f);

    // Mojo parameter smoothing
    smoothedVariance.setTargetValue(varianceParam / 100.0f);
    smoothedNoise.setTargetValue(noiseParam / 100.0f);

    // Professional tools
    smoothedFocus.setTargetValue(focusParam);
    deltaMonitor.setActive(deltaMode);

    // Harmonic entropy
    smoothedEntropy.setTargetValue(entropyParam / 100.0f);

    // Network Control сглаживание
    smoothedNetDepth.setTargetValue(pDepth / 100.0f);
    smoothedNetSens.setTargetValue(pSens / 100.0f);


    // --- 2. СЕТЬ (На обычной частоте) ---

    currentGroup = (int)*apvts.getRawParameterValue("group_id");
    isReference  = (*apvts.getRawParameterValue("role") > 0.5f);
    int modeIndex = *apvts.getRawParameterValue("mode");

    // Если Reference -> Анализируем вход и шлем в сеть
    if (isReference) {
        float maxPeak = buffer.getMagnitude(0, originalNumSamples);
        float envValue = bandEnvelopes[0].process(maxPeak);
        NetworkManager::getInstance().updateBandSignal(currentGroup, 0, envValue);
    }

    // Если Listener -> Читаем из сети
    float rawNetVal = (!isReference) ? NetworkManager::getInstance().getBandSignal(currentGroup, 0) : 0.0f;
    smoothedNetworkSignal.setTargetValue(rawNetVal);

    // --- GLOBAL HEAT LOGIC ---

    // 1. Измеряем свою энергию (RMS входа)
    float myRMS = 0.0f;
    for(int ch = 0; ch < numCh; ++ch) myRMS += buffer.getRMSLevel(ch, 0, originalNumSamples);
    myRMS /= (float)numCh;

    // 2. Отправляем в сеть
    if (myInstanceIndex != -1) {
        NetworkManager::getInstance().updateInstanceEnergy(myInstanceIndex, myRMS);
    }

    // 3. Читаем общую температуру
    float globalEnergy = NetworkManager::getInstance().getGlobalHeat();

    // Вычитаем себя из общей суммы (чтобы не фидбечить на самого себя)
    float otherEnergy = std::max(0.0f, globalEnergy - myRMS);

    // Масштабируем (эмпирически): допустим 5 треков = 2.5, нормируем к 0..1
    float heatTarget = std::min(1.0f, otherEnergy / 5.0f);

    // Применяем ручку Amount
    heatTarget *= (heatParam / 100.0f);

    smoothedGlobalHeat.setTargetValue(heatTarget);

    // === ANALOG DRIFT PREPARATION ===
    float driftAmount = smoothedAnalogDrift.getNextValue();

    // --- 3. DRY COPY (На обычной частоте) ---

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);
    juce::dsp::AudioBlock<float> dryBlock(dryBuffer);
    juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);
    dryDelayLine.process(dryContext);

    // --- 4. UPSAMPLING (Взлетаем!) ---

    // Создаем AudioBlock из входного буфера
    juce::dsp::AudioBlock<float> inputBlock(buffer);

    // Апсемплинг!
    auto upsampledBlock = oversampler->processSamplesUp(inputBlock);

    int numSamplesHigh = (int)upsampledBlock.getNumSamples();

    // Создаем временный буфер для суммы Wet сигнала
    juce::AudioBuffer<float> wetSumBufferHigh;
    wetSumBufferHigh.setSize(numCh, numSamplesHigh);
    wetSumBufferHigh.clear();

    // Создаем массив указателей для FilterBank
    float* upChannels[2];
    upChannels[0] = upsampledBlock.getChannelPointer(0);
    upChannels[1] = (numCh > 1) ? upsampledBlock.getChannelPointer(1) : nullptr;
    juce::AudioBuffer<float> upWrapper(upChannels, numCh, numSamplesHigh);

    // === PRE-FILTERING (TIGHTEN) ===
    // Фильтруем сигнал ПЕРЕД сплитом (убираем грязь на низах)
    for (int i = 0; i < numSamplesHigh; ++i)
    {
        float cutoff = smoothedTightenFreq.getNextValue();

        for (int ch = 0; ch < numCh; ++ch) {
            preFilters[ch].setCutoffFrequency(cutoff);
            float* data = upsampledBlock.getChannelPointer(ch);
            data[i] = preFilters[ch].processSample(0, data[i]);
        }
    }

    // --- 5. SPLIT (High Rate) ---
    filterBank->splitIntoBands(upWrapper, bandBufferPtrs.data(), numSamplesHigh);

    // --- 6. PROCESS LOOP (High Rate) ---

    // Фиксированный Tilt (меньше грязи на низах)


    auto* dryL = dryBuffer.getReadPointer(0);
    auto* dryR = (numCh > 1) ? dryBuffer.getReadPointer(1) : nullptr;
    auto* finalL = buffer.getWritePointer(0);
    auto* finalR = (numCh > 1) ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamplesHigh; ++i)
    {
        // === GLOBAL VOLTAGE STARVATION ===
        // Вычисляем просадку питания (используем globalEnergy из выше)
        float starvationMult = psu.process(globalEnergy, driftAmount);

        // === STEREO VARIANCE ===
        // Получаем текущие значения variance и noise
        float currentVariance = smoothedVariance.getNextValue();
        float currentNoise = smoothedNoise.getNextValue();

        // Вычисляем дрейф drive для L/R каналов
        auto drift = stereoDrift.getDrift(currentVariance);

        // === NETWORK CONTROL PROCESSING (The Holy Trinity) ===
        float depth = smoothedNetDepth.getNextValue();
        // Базовые параметры
        float baseDrive = smoothedDrive.getNextValue();
        float mixVal    = smoothedMix.getNextValue();
        float outG      = smoothedOutput.getNextValue();

        // === GLOBAL HEAT APPLICATION ===
        // Получаем текущую "температуру" микса
        float heatVal = smoothedGlobalHeat.getNextValue();


        // === HARMONIC ENTROPY ===
        float entropyAmount = smoothedEntropy.getNextValue();

        // === PUNCH (TRANSIENT CONTROL) ===
        // Получаем текущий punch параметр (-1..1)
        float punchVal = smoothedPunch.getNextValue();

        float wetSampleL = 0.0f;
        float wetSampleR = 0.0f;


        for (int band = 0; band < kNumBands; ++band)
        {
            // === SPLIT & CRUSH (PUNCH) ===
            // Физическое разделение сигнала на Transient и Body
            float xL = bandBuffers[band].getSample(0, i);
            float xR = (numCh > 1) ? bandBuffers[band].getSample(1, i) : xL;

            // === THERMAL MODELING ===
            // Добавляем тепловой bias перед сатурацией
            float biasL = tubes[band][0].process(xL) * driftAmount;
            float biasR = tubes[band][1].process(xR) * driftAmount;

            // === HARMONIC ENTROPY ===
            // Добавляем стохастический дрейф смещения
            float entropyDriftL = entropyModules[band][0].process(entropyAmount);
            float entropyDriftR = entropyModules[band][1].process(entropyAmount);

            // Применяем bias к входным сэмплам
            float inputL = xL + biasL + entropyDriftL;
            float inputR = xR + biasR + entropyDriftR;


            // 4. Применяем Global Heat к драйву
            float heatDrive = baseDrive * (1.0f + heatVal * 0.5f); // До +50% драйва в пике микса

            // === SPLIT & CRUSH (PUNCH) ===
            // Физическое разделение сигнала на Transient и Body

            // 1. SPLIT: Разделяем сигнал на Transient и Body
            auto splitL = splitters[band][0].process(inputL);
            auto splitR = splitters[band][1].process(inputR);

            // 2. PROCESS BODY (Всегда жирный, как выбрала Математика Вселенной)
            float processedBodyL = mathShapers[band].processSample(splitL.body, heatDrive, currentMathMode);
            float processedBodyR = mathShapers[band].processSample(splitR.body, heatDrive, currentMathMode);

            // 3. PROCESS TRANSIENT (Зависит от Punch)
            float processedTransL = 0.0f;
            float processedTransR = 0.0f;

            if (punchVal > 0.01f)
            {
                // === HARD PUNCH (Positive) ===
                // Атака становится жесткой и агрессивной
                float transDrive = heatDrive * (1.0f + punchVal * 2.0f); // До 3x драйва на атаку
                processedTransL = mathShapers[band].processSample(splitL.trans, transDrive, currentMathMode);
                processedTransR = mathShapers[band].processSample(splitR.trans, transDrive, currentMathMode);
            }
            else if (punchVal < -0.01f)
            {
                // === CLEAN PUNCH (Negative) ===
                // Атака остается чистой (сохраняет динамику)
                float transDrive = heatDrive * (1.0f - std::abs(punchVal) * 0.8f); // Снижаем драйв
                processedTransL = mathShapers[band].processSample(splitL.trans, transDrive, MathMode::EulerTube); // Clean - используем Euler для мягкости
                processedTransR = mathShapers[band].processSample(splitR.trans, transDrive, MathMode::EulerTube);
            }
            else
            {
                // === NEUTRAL (Punch = 0) ===
                // Атака обрабатывается так же, как тело
                processedTransL = mathShapers[band].processSample(splitL.trans, heatDrive, currentMathMode);
                processedTransR = mathShapers[band].processSample(splitR.trans, heatDrive, currentMathMode);
            }

            // 4. SUM: Склеиваем Transient и Body обратно
            float combinedL = processedBodyL + processedTransL;
            float combinedR = processedBodyR + processedTransR;

            // 5. Применяем Stereo Variance к результату
            combinedL *= drift.driveMultL;
            combinedR *= drift.driveMultR;

            // 6. Predictive Compensation для Split & Crush
            float predComp = 1.0f / std::sqrt(std::max(1.0f, heatDrive));
            if (currentMathMode == MathMode::PiFold)
                predComp *= 1.2f;

            wetSampleL += combinedL * predComp;
            wetSampleR += combinedR * predComp;
        }

        // === POST-FILTERING (SMOOTH) ===
        // Фильтруем сумму полос перед финальным миксом
        {
            float cutoff = smoothedSmoothFreq.getNextValue();

            // Фильтруем левый канал
            postFilters[0].setCutoffFrequency(cutoff);
            wetSampleL = postFilters[0].processSample(0, wetSampleL);

            // Фильтруем правый канал
            if (numCh > 1) {
                postFilters[1].setCutoffFrequency(cutoff);
                wetSampleR = postFilters[1].processSample(0, wetSampleR);
            }
        }

        // === STEREO CROSSTALK ===
        // Взаимопроникновение каналов для "склейки" стерео
        stereoDrift.applyCrosstalk(wetSampleL, wetSampleR, currentVariance);

        // === NOISE BREATHER ===
        // Добавляем дышаний шум
        float signalLevel = (std::abs(wetSampleL) + std::abs(wetSampleR)) * 0.5f;
        float noiseSample = noiseFloor.getNoiseSample(signalLevel, currentNoise);

        // Подмешиваем шум к обоим каналам
        wetSampleL += noiseSample;
        wetSampleR += noiseSample;

        // === PSYCHOACOUSTIC AUTO-GAIN ===
        // Скармливаем Dry (эталон) и Wet (грязный). Получаем множитель.
        float dryL_s = dryL[i];
        float dryR_s = dryR ? dryR[i] : 0.0f;

        float psychoScale = psychoGain.processStereoSample(dryL_s, dryR_s, wetSampleL, wetSampleR);

        // Применяем "умный" гейн
        wetSampleL *= psychoScale;
        wetSampleR *= psychoScale;

        // === MIX & OUTPUT ===
        float outL_val = dryL_s * (1.0f - mixVal) + wetSampleL * mixVal;
        outL_val *= outG;

        // Soft Clip (Мастеринг-качество)
        // Используем tanh для пиков > 1.0, чтобы не было цифры
        if (std::abs(outL_val) > 1.0f) outL_val = std::tanh(outL_val);

        if (finalR) {
            float outR_val = dryR_s * (1.0f - mixVal) + wetSampleR * mixVal;
            outR_val *= outG;
            if (std::abs(outR_val) > 1.0f) outR_val = std::tanh(outR_val);

            // === DELTA MONITORING ===
            finalL[i] = deltaMonitor.process(dryL_s, 0.0f, outL_val);
            finalR[i] = deltaMonitor.process(dryR_s, 0.0f, outR_val);
        } else {
            // === DELTA MONITORING ===
            finalL[i] = deltaMonitor.process(dryL_s, 0.0f, outL_val);
        }

        // === SOFT START FADE ===
        // Получаем множитель (от 0.0 до 1.0)
        // После первых 200мс он всегда будет равен 1.0 и перестанет влиять на звук.
        float fade = startupFader.getNextValue();

        finalL[i] *= fade;
        if (finalR) finalR[i] *= fade;
    }
    
    // Очистка
    for (int i = numCh; i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, originalNumSamples);
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

