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

    // Диапазон от -6 dB (чисто) до +24 dB (грязь)
    // Дефолт ставим 0.0
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("drive_master", 1), "Master Drive",
        juce::NormalisableRange<float>(-6.0f, 24.0f, 0.1f), 0.0f));

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
    dryDelayLine.prepare({sampleRate, (juce::uint32)samplesPerBlock, 2}); // Stereo
    dryDelayLine.setMaximumDelayInSamples(2048); // С запасом
    dryDelayLine.setDelay((float)latency); // Выравниваем Dry под Wet

    // 3. Буферы (как было)
    bandBufferPtrs.resize(kNumBands);
    for (int i = 0; i < kNumBands; ++i) {
        bandBuffers[i].setSize(2, samplesPerBlock);
        bandBufferPtrs[i] = &bandBuffers[i];
    }

    // 4. Сглаживание
    smoothedDrive.reset(sampleRate, 0.05);  // 50ms
    smoothedOutput.reset(sampleRate, 0.05);
    smoothedMix.reset(sampleRate, 0.05);

    smoothedDrive.setCurrentAndTargetValue(0.0f);
    smoothedOutput.setCurrentAndTargetValue(1.0f);
    smoothedMix.setCurrentAndTargetValue(1.0f);

    // Сглаживание сети
    smoothedNetworkSignal.reset(sampleRate, 0.02);

    envelope.reset(sampleRate);
}

void CoheraSaturatorAudioProcessor::releaseResources()
{
    // Освобождаем ресурсы, если нужно (unique_ptr сделает это сам)
    filterBank->reset();
}

void CoheraSaturatorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int totalNumInputChannels  = getTotalNumInputChannels();
    const int numSamples = buffer.getNumSamples();

    // --- 1. Подготовка параметров ---

    float driveParam = *apvts.getRawParameterValue("drive_master"); // 0..100
    float mixParam   = *apvts.getRawParameterValue("mix");          // 0..100
    float outDb      = *apvts.getRawParameterValue("output_gain");

    // Маппинг Drive: 0 -> 1.0 (clean), 100 -> 16.0 (+24dB dirt)
    float targetDriveRatio = 1.0f + (driveParam * 0.15f);
    // Маппинг Mix: 0..100 -> 0..1
    float targetMix = mixParam / 100.0f;

    smoothedDrive.setTargetValue(targetDriveRatio);
    smoothedMix.setTargetValue(targetMix);
    smoothedOutput.setTargetValue(juce::Decibels::decibelsToGain(outDb));

    // Сетевые дела (оставляем как есть, это работает)
    currentGroup = (int)*apvts.getRawParameterValue("group_id");
    isReference  = (*apvts.getRawParameterValue("role") > 0.5f);

    if (isReference) {
        float maxPeak = buffer.getMagnitude(0, numSamples);
        float envValue = envelope.process(maxPeak);
        NetworkManager::getInstance().updateGroupSignal(currentGroup, envValue);
    }

    float targetNetValue = (!isReference) ? NetworkManager::getInstance().getGroupSignal(currentGroup) : 0.0f;
    smoothedNetworkSignal.setTargetValue(targetNetValue);

    // === 2. СОЗДАЕМ DRY КОПИЮ ===

    // Копируем входной буфер, чтобы потом смешать его с обработанным
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    // Прогоняем Dry через задержку, равную задержке фильтров
    // Это ВАЖНО, иначе при миксе (50/50) будет гребенчатый фильтр (то самое "тускло")
    juce::dsp::AudioBlock<float> dryBlock(dryBuffer);
    juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);
    dryDelayLine.process(dryContext);

    // === 3. ОБРАБОТКА (WET) ===

    // Сначала измерим RMS входа (для авто-гейна)
    float inRms = buffer.getRMSLevel(0, 0, numSamples); // RMS левого канала для примера

    // 3.1 Split
    filterBank->splitIntoBands(buffer, bandBufferPtrs.data(), numSamples);

    // 3.2 Saturate
    float currentDrive = smoothedDrive.getNextValue(); smoothedDrive.skip(numSamples-1);
    float netVal = smoothedNetworkSignal.getNextValue(); smoothedNetworkSignal.skip(numSamples-1);

    // Network Modulation (Inverse Grime logic)
    float dynamicMod = 1.0f;
    if (!isReference && netVal > 0.0f) {
        dynamicMod = 1.0f - (netVal * 0.5f); // Ducking 50%
        if (dynamicMod < 0.0f) dynamicMod = 0.0f;
    }

    float effectiveDrive = currentDrive * dynamicMod;

    for (int band = 0; band < kNumBands; ++band)
    {
        for (int ch = 0; ch < 2; ++ch)
        {
            float* data = bandBuffers[band].getWritePointer(ch);
            for (int i = 0; i < numSamples; ++i)
            {
                // Линейная зона tanh - это x < 0.5.
                // Если effectiveDrive близок к 1.0, tanh(x) почти равен x.
                // Это дает чистый звук на старте.
                float x = data[i] * effectiveDrive;
                data[i] = std::tanh(x);
            }
        }
    }

    // 3.3 Sum (Сборка Wet сигнала)
    buffer.clear();
    for (int ch = 0; ch < 2; ++ch)
    {
        auto* out = buffer.getWritePointer(ch);
        for (int band = 0; band < kNumBands; ++band) {
            juce::FloatVectorOperations::add(out, bandBuffers[band].getReadPointer(ch), numSamples);
        }
    }

    // === 4. AUTO-GAIN (RMS MATCHING) ===

    // Измеряем RMS того, что получилось
    float outRms = buffer.getRMSLevel(0, 0, numSamples);

    // Защита от деления на ноль
    if (outRms < 0.0001f) outRms = 0.0001f;
    if (inRms < 0.0001f) inRms = outRms; // Если тишина на входе, не бустим шум

    // Вычисляем коэффициент компенсации
    // Плавно (через фильтр), чтобы громкость не скакала
    float targetComp = inRms / outRms;

    // Ограничиваем компенсацию, чтобы не было взрыва (макс +12dБ, мин -24дБ)
    targetComp = juce::jlimit(0.1f, 4.0f, targetComp);

    // Применяем компенсацию к Wet сигналу
    buffer.applyGain(targetComp);

    // --- 5. Sum ---
    buffer.clear();
    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto* outData = buffer.getWritePointer(ch);
        for (int band = 0; band < kNumBands; ++band)
        {
            const auto* bandData = bandBuffers[band].getReadPointer(ch);
            juce::FloatVectorOperations::add(outData, bandData, numSamples);
        }

        // === 5. MIX & OUTPUT ===

        float currentMix = smoothedMix.getNextValue(); smoothedMix.skip(numSamples-1);
        float currentOutGain = smoothedOutput.getNextValue(); smoothedOutput.skip(numSamples-1);

        for (int ch = 0; ch < 2; ++ch)
        {
            auto* wetData = buffer.getWritePointer(ch);
            const auto* dryData = dryBuffer.getReadPointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                // Dry/Wet Mix
                // 0% = Чистый задержанный Dry
                // 100% = Сатурированный RMS-согласованный Wet
                float mixed = dryData[i] * (1.0f - currentMix) + wetData[i] * currentMix;

                // Output Gain
                mixed *= currentOutGain;

                // Final Safety Limiter
                // Жесткий потолок 0dBFS, чтобы не краснело
                if (mixed > 1.0f) mixed = 1.0f;
                else if (mixed < -1.0f) mixed = -1.0f;

                wetData[i] = mixed;
            }
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
