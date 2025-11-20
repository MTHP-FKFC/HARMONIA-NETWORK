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
    // Гарантируем, что работаем минимум со стерео, если хост дает моно - дублируем логику
    const int numCh = juce::jmin(buffer.getNumChannels(), 2);

    // --- 1. ПАРАМЕТРЫ ---

    float driveParam = *apvts.getRawParameterValue("drive_master");
    float mixParam   = *apvts.getRawParameterValue("mix");
    float outDb      = *apvts.getRawParameterValue("output_gain");

    float targetDrive = 1.0f + (driveParam * 0.15f);
    float targetMix = mixParam / 100.0f; // 0..1

    smoothedDrive.setTargetValue(targetDrive);
    smoothedMix.setTargetValue(targetMix);
    smoothedOutput.setTargetValue(juce::Decibels::decibelsToGain(outDb));

    // --- 2. СЕТЬ ---

    currentGroup = (int)*apvts.getRawParameterValue("group_id");
    isReference  = (*apvts.getRawParameterValue("role") > 0.5f);

    // Анализ ВХОДА для авто-гейна и сети (сумма L+R)
    float inEnergy = 0.0f;
    for (int ch = 0; ch < numCh; ++ch)
        inEnergy += buffer.getMagnitude(ch, 0, numSamples);
    inEnergy /= (float)numCh;

    float currentInputLevel = inputLevelFollower.process(inEnergy);

    if (isReference) {
        NetworkManager::getInstance().updateGroupSignal(currentGroup, currentInputLevel);
    }
    float targetNetValue = (!isReference) ? NetworkManager::getInstance().getGroupSignal(currentGroup) : 0.0f;
    smoothedNetworkSignal.setTargetValue(targetNetValue);

    // --- 3. DRY КОПИЯ (для фазовой компенсации) ---

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    juce::dsp::AudioBlock<float> dryBlock(dryBuffer);
    juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);
    dryDelayLine.process(dryContext);

    // --- 4. SPLIT ---

    filterBank->splitIntoBands(buffer, bandBufferPtrs.data(), numSamples);

    // --- 5. ГЛАВНЫЙ ЦИКЛ ОБРАБОТКИ (ПОСЭМПЛОВЫЙ) ---

    // Здесь мы считаем управляющие сигналы ОДИН РАЗ и применяем ко ВСЕМ каналам

    // Для измерения выхода (для автогейна следующего блока)
    float accumulatedWetEnergy = 0.0f;

    // Указатели на данные
    // Предполагаем стерео (или моно). Для безопасности берем указатели заранее.
    auto* dryL = dryBuffer.getReadPointer(0);
    auto* dryR = (numCh > 1) ? dryBuffer.getReadPointer(1) : nullptr;

    auto* outL = buffer.getWritePointer(0);
    auto* outR = (numCh > 1) ? buffer.getWritePointer(1) : nullptr;

    // Предварительно рассчитываем таргет компенсации на основе ПРЕДЫДУЩЕГО блока
    // (Это стандартная практика для Auto-Gain, чтобы избежать задержек в цепи управления)
    // Берем последнее значение детектора выхода
    float currentWetLevel = outputLevelFollower.getCurrentValue();
    float targetComp = 1.0f;
    if (currentWetLevel > 0.001f && currentInputLevel > 0.001f) {
        targetComp = currentInputLevel / currentWetLevel;
    }
    targetComp = juce::jlimit(0.06f, 4.0f, targetComp);
    smoothedCompensation.setTargetValue(targetComp);

    for (int i = 0; i < numSamples; ++i)
    {
        // 5.1 Обновляем управляющие сигналы (ОДИН РАЗ для стерео-пары)
        float driveVal = smoothedDrive.getNextValue();
        float netVal   = smoothedNetworkSignal.getNextValue();
        float mixVal   = smoothedMix.getNextValue();
        float outGain  = smoothedOutput.getNextValue();
        float compVal  = smoothedCompensation.getNextValue();

        // 5.2 Логика Ducking (Network)
        float dynamicMod = 1.0f;
        if (!isReference && netVal > 0.0f) {
            dynamicMod = 1.0f - (netVal * 0.5f);
            if (dynamicMod < 0.0f) dynamicMod = 0.0f;
        }
        float effectiveDrive = driveVal * dynamicMod;

        // 5.3 Суммирование полос (WET)
        float wetSampleL = 0.0f;
        float wetSampleR = 0.0f;

        for (int band = 0; band < kNumBands; ++band)
        {
            // Левый канал
            float xL = bandBuffers[band].getSample(0, i);
            xL *= effectiveDrive;
            wetSampleL += std::tanh(xL); // Суммируем сатурированную полосу

            // Правый канал (если есть)
            if (outR) {
                float xR = bandBuffers[band].getSample(1, i);
                xR *= effectiveDrive;
                wetSampleR += std::tanh(xR);
            }
        }

        // 5.4 Auto-Gain на Wet сигнал (применяем одинаково к L и R)
        wetSampleL *= compVal;
        if (outR) wetSampleR *= compVal;

        // Собираем статистику для авто-гейна (абсолютные значения)
        accumulatedWetEnergy += std::abs(wetSampleL);
        if (outR) accumulatedWetEnergy += std::abs(wetSampleR);

        // 5.5 Mix & Output
        // Left
        float finalL = dryL[i] * (1.0f - mixVal) + wetSampleL * mixVal;
        finalL *= outGain;
        finalL = std::tanh(finalL); // Safety Limiter
        outL[i] = finalL;

        // Right
        if (outR) {
            float finalR = dryR[i] * (1.0f - mixVal) + wetSampleR * mixVal;
            finalR *= outGain;
            finalR = std::tanh(finalR); // Safety Limiter
            outR[i] = finalR;
        }
    }

    // 6. Обновляем детектор выхода (для следующего блока)
    float avgWetEnergy = accumulatedWetEnergy / (float)(numSamples * numCh);
    outputLevelFollower.process(avgWetEnergy);

    // Очистка мусора в неиспользуемых каналах
    for (int i = numCh; i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, numSamples);
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
