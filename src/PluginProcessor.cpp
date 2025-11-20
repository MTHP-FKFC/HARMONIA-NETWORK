#include "PluginProcessor.h"
#include "PluginEditor.h"

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

    // Group & Role
    layout.add(std::make_unique<juce::AudioParameterInt>("group_id", "Group ID", 0, 7, 0));
    layout.add(std::make_unique<juce::AudioParameterChoice>("role", "Role", juce::StringArray{"Listener", "Reference"}, 0));

    return layout;
}

void CoheraSaturatorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // 1. Фильтры (как было)
    FilterBankConfig config;
    config.sampleRate = sampleRate;
    config.maxBlockSize = (juce::uint32)samplesPerBlock;
    config.numBands = kNumBands;
    config.phaseMode = FilterPhaseMode::LinearFIR256;
    config.profile = CrossoverProfile::Default;
    filterBank->prepare(config);

    // ВАЖНО: Получаем задержку фильтров
    int latency = filterBank->getLatencySamples();
    setLatencySamples(latency);

    // 2. Инициализируем Dry Delay Line (чтобы фаза совпадала)
    dryDelayLine.prepare({sampleRate, (juce::uint32)samplesPerBlock, (juce::uint32)getTotalNumInputChannels()});
    dryDelayLine.setDelay((float)latency); // Выравниваем Dry под Wet

    // 3. Буферы (как было)
    bandBufferPtrs.resize(kNumBands);
    for (int i = 0; i < kNumBands; ++i) {
        bandBuffers[i].setSize(2, samplesPerBlock);
        bandBufferPtrs[i] = &bandBuffers[i];
    }

    // 4. Сглаживание параметров
    smoothedDrive.reset(sampleRate, 0.05);
    smoothedOutput.reset(sampleRate, 0.05);
    smoothedMix.reset(sampleRate, 0.05);

    smoothedDrive.setCurrentAndTargetValue(1.0f);
    smoothedOutput.setCurrentAndTargetValue(1.0f);
    smoothedMix.setCurrentAndTargetValue(1.0f); // По дефолту Wet, чтобы слышать эффект

    smoothedNetworkSignal.reset(sampleRate, 0.02);

    // 5. НОВОЕ: Настройка детекторов уровня для Авто-Гейна
    // Ставим медленный релиз (300мс), чтобы громкость не "пампила"
    inputLevelFollower.reset(sampleRate);
    outputLevelFollower.reset(sampleRate);

    // Сглаживание самой компенсации (медленное, 100мс), чтобы не было резких скачков
    smoothedCompensation.reset(sampleRate, 0.1);
    smoothedCompensation.setCurrentAndTargetValue(1.0f);

    envelope.reset(sampleRate); // Для сети
}

void CoheraSaturatorAudioProcessor::releaseResources()
{
    // Освобождаем ресурсы, если нужно (unique_ptr сделает это сам)
    filterBank->reset();
}

void CoheraSaturatorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int totalNumInputChannels = getTotalNumInputChannels(); // Важно для стерео!

    // --- 1. Параметры ---

    float driveParam = *apvts.getRawParameterValue("drive_master");
    float mixParam   = *apvts.getRawParameterValue("mix");
    float outDb      = *apvts.getRawParameterValue("output_gain");

    // Маппинг Drive: 0% -> 1.0 (чисто), 100% -> 16.0 (+24dB жир)
    float targetDrive = 1.0f + (driveParam * 0.15f);
    float targetMix = mixParam / 100.0f;

    smoothedDrive.setTargetValue(targetDrive);
    smoothedMix.setTargetValue(targetMix);
    smoothedOutput.setTargetValue(juce::Decibels::decibelsToGain(outDb));

    // Сеть
    currentGroup = (int)*apvts.getRawParameterValue("group_id");
    isReference = (*apvts.getRawParameterValue("role") > 0.5f);

    // --- 2. АНАЛИЗ ВХОДА (Стерео RMS) ---

    // ФИКС КАНАЛОВ: Считаем энергию по ВСЕМ каналам сразу
    float inEnergy = 0.0f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        // RMS или Peak - для автогейна лучше RMS, но быстрый
        inEnergy += buffer.getMagnitude(ch, 0, numSamples);
    }
    inEnergy /= (float)totalNumInputChannels; // Среднее по каналам

    // Обновляем инерционный детектор входа
    float currentInputLevel = inputLevelFollower.process(inEnergy);

    // Отправка в сеть (если референс)
    if (isReference) {
        NetworkManager::getInstance().updateGroupSignal(currentGroup, currentInputLevel);
    }
    float targetNetValue = (!isReference) ? NetworkManager::getInstance().getGroupSignal(currentGroup) : 0.0f;
    smoothedNetworkSignal.setTargetValue(targetNetValue);

    // --- 3. КОПИЯ DRY (для параллельной обработки) ---
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // Задержка Dry для фазовой когерентности
    juce::dsp::AudioBlock<float> dryBlock(dryBuffer);
    juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);
    dryDelayLine.process(dryContext);

    // --- 4. ОБРАБОТКА WET (Сатурация) ---

    // 4.1 Разделение
    filterBank->splitIntoBands(buffer, bandBufferPtrs.data(), numSamples);

    // 4.2 Сатурация
    float currentDrive = smoothedDrive.getNextValue(); smoothedDrive.skip(numSamples-1);
    float netVal = smoothedNetworkSignal.getNextValue(); smoothedNetworkSignal.skip(numSamples-1);

    // Inverse Grime (Ducking)
    float dynamicMod = 1.0f;
    if (!isReference && netVal > 0.0f) {
        dynamicMod = 1.0f - (netVal * 0.5f);
        if (dynamicMod < 0.0f) dynamicMod = 0.0f;
    }
    float effectiveDrive = currentDrive * dynamicMod;

    for (int band = 0; band < kNumBands; ++band)
    {
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            float* data = bandBuffers[band].getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                // Сатурация
                float x = data[i];
                x *= effectiveDrive;
                data[i] = std::tanh(x);
            }
        }
    }

    // 4.3 Сборка Wet
    buffer.clear();
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto* out = buffer.getWritePointer(ch);
        for (int band = 0; band < kNumBands; ++band) {
            juce::FloatVectorOperations::add(out, bandBuffers[band].getReadPointer(ch), numSamples);
        }
    }

    // --- 5. ИНЕРЦИОННЫЙ AUTO-GAIN (ФИКС СТРАТОСФЕРЫ) ---

    // 5.1 Измеряем уровень "Грязного" сигнала (тоже стерео-среднее)
    float wetEnergy = 0.0f;
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        wetEnergy += buffer.getMagnitude(ch, 0, numSamples);
    }
    wetEnergy /= (float)totalNumInputChannels;

    // 5.2 Обновляем детектор выхода
    float currentWetLevel = outputLevelFollower.process(wetEnergy);

    // 5.3 Вычисляем целевую компенсацию
    // Target = InputLevel / WetLevel
    // ФИКС: Добавляем порог тишины (0.001), чтобы не делить на ноль и не разгонять шум
    float targetComp = 1.0f;

    if (currentWetLevel > 0.001f && currentInputLevel > 0.001f)
    {
        targetComp = currentInputLevel / currentWetLevel;
    }

    // ФИКС: Ограничиваем компенсацию разумными пределами
    // Не даем усиливать больше чем на +12dB (x4) и давить сильнее -24dB (x0.06)
    targetComp = juce::jlimit(0.06f, 4.0f, targetComp);

    // 5.4 Сглаживаем компенсацию
    smoothedCompensation.setTargetValue(targetComp);

    // Применяем сглаженную компенсацию (посэмплово или поблочно)
    // Здесь для экономии применим поблочно (ramp), так как smoothedCompensation обновляется медленно
    smoothedCompensation.applyGain(buffer, numSamples);

    // --- 6. МИКС И ВЫХОД ---

    float currentMix = smoothedMix.getNextValue(); smoothedMix.skip(numSamples-1);
    float currentOutGain = smoothedOutput.getNextValue(); smoothedOutput.skip(numSamples-1);

    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto* wetData = buffer.getWritePointer(ch);
        const auto* dryData = dryBuffer.getReadPointer(ch);

        for (int i = 0; i < numSamples; ++i)
        {
            // Dry/Wet
            // Важно: Wet уже скомпенсирован по уровню к Dry!
            float wet = wetData[i];
            float dry = dryData[i];

            // Equal Power Crossfade (опционально, здесь линейный для прозрачности фазы)
            // Линейный микс: Dry * (1-Mix) + Wet * Mix
            float mixed = dry * (1.0f - currentMix) + wet * currentMix;

            // Output Gain
            mixed *= currentOutGain;

            // Soft Clip на мастере (чтобы не клиповало в DAW)
            if (mixed > 1.0f) mixed = std::tanh(mixed);
            else if (mixed < -1.0f) mixed = std::tanh(mixed);

            wetData[i] = mixed;
        }
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
