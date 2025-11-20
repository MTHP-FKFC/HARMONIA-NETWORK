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

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID("output_gain", 1), "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 6.0f, 0.1f), 0.0f));

    // Группа (0-7)
    layout.add(std::make_unique<juce::AudioParameterInt>(
        "group_id", "Group ID", 0, 7, 0));

    // Роль: 0 = Listener, 1 = Reference
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "role", "Role", juce::StringArray{"Listener", "Reference"}, 0));

    return layout;
}

void CoheraSaturatorAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // 1. Готовим фильтры
    FilterBankConfig config;
    config.sampleRate = sampleRate;
    config.maxBlockSize = (juce::uint32)samplesPerBlock;
    config.numBands = kNumBands;
    config.phaseMode = FilterPhaseMode::LinearFIR256; // Максимальное качество
    config.profile = CrossoverProfile::Default;

    filterBank->prepare(config);

    // Устанавливаем задержку (Latency Reporting)
    setLatencySamples(filterBank->getLatencySamples());

    // 2. Готовим буферы полос
    bandBufferPtrs.resize(kNumBands);

    for (int i = 0; i < kNumBands; ++i)
    {
        // 2 канала, размер блока
        bandBuffers[i].setSize(2, samplesPerBlock);
        // Сохраняем указатель для API FilterBank
        bandBufferPtrs[i] = &bandBuffers[i];
    }

    // 3. Сглаживание (FIXED)
    // 100 мс сглаживания - плавно, но не вечность
    smoothedDrive.reset(sampleRate, 0.1);
    smoothedOutput.reset(sampleRate, 0.1);
    smoothedNetworkSignal.reset(sampleRate, 0.01); // 10ms reaction time

    smoothedDrive.setCurrentAndTargetValue(1.0f);
    smoothedOutput.setCurrentAndTargetValue(1.0f);
    smoothedNetworkSignal.setCurrentAndTargetValue(0.0f);

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

    // --- 1. Читаем параметры ---

    float driveParam = *apvts.getRawParameterValue("drive_master"); // Теперь это просто дБ (-6..+24)
    float outDb      = *apvts.getRawParameterValue("output_gain");

    // Параметры сети
    currentGroup = (int)*apvts.getRawParameterValue("group_id");
    isReference  = (*apvts.getRawParameterValue("role") > 0.5f);

    // --- 2. Сеть (Network Logic) ---

    if (isReference)
    {
        float maxPeak = buffer.getMagnitude(0, numSamples);
        float envValue = envelope.process(maxPeak);
        NetworkManager::getInstance().updateGroupSignal(currentGroup, envValue);
    }

    float targetNetValue = 0.0f;
    if (!isReference)
    {
        targetNetValue = NetworkManager::getInstance().getGroupSignal(currentGroup);
    }
    smoothedNetworkSignal.setTargetValue(targetNetValue);

    // --- 3. Gain Staging ---

    // HEADROOM (Запас): Ослабляем вход на -6dB, чтобы сумма 6 полос не клиповала
    // При сложении полос уровень вернется в норму.
    float headroom = 0.5f;

    // Drive: Преобразуем dB в разы.
    // -6dB = 0.5, 0dB = 1.0, +24dB = 15.8
    float targetDriveRatio = juce::Decibels::decibelsToGain(driveParam);

    smoothedDrive.setTargetValue(targetDriveRatio);
    smoothedOutput.setTargetValue(juce::Decibels::decibelsToGain(outDb));

    float currentDrive = smoothedDrive.getNextValue();
    smoothedDrive.skip(numSamples - 1);
    float currentOut = smoothedOutput.getNextValue();
    smoothedOutput.skip(numSamples - 1);

    // --- 4. Обработка ---

    // Очистка выходов
    for (auto i = totalNumInputChannels; i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, numSamples);

    // Применяем Headroom ко всему входу
    buffer.applyGain(headroom);

    // Split
    filterBank->splitIntoBands(buffer, bandBufferPtrs.data(), numSamples);

    for (int i = 0; i < numSamples; ++i)
    {
        float netValue = smoothedNetworkSignal.getNextValue();

        // Логика Ducking (Inverse Grime)
        // Если сеть активна (netValue > 0), мы уменьшаем Drive
        float dynamicMod = 1.0f;
        if (!isReference && netValue > 0.0f)
        {
            dynamicMod = 1.0f - (netValue * 0.5f); // Depth 50%
            if (dynamicMod < 0.0f) dynamicMod = 0.0f;
        }

        // Итоговый Drive для этого сэмпла
        float effectiveDrive = currentDrive * dynamicMod;

        // Auto-Gain Compensation
        // Если мы бустим вход в N раз, мы делим выход на N (примерно),
        // чтобы громкость не менялась, а менялась только "плотность".
        // Добавляем защитный порог 1.0f, чтобы не делить на дроби (не бустить тихие звуки).
        float comp = 1.0f / std::max(1.0f, effectiveDrive);

        for (int band = 0; band < kNumBands; ++band)
        {
            for (int ch = 0; ch < totalNumInputChannels; ++ch)
            {
                float* samplePtr = bandBuffers[band].getWritePointer(ch) + i;
                float x = *samplePtr;

                // 1. Drive
                x *= effectiveDrive;

                // 2. Saturation
                x = std::tanh(x);

                // 3. Compensation
                x *= comp;

                *samplePtr = x;
            }
        }
    }

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

        // Убираем Headroom (возвращаем +6dB) и применяем Output Gain
        // (1.0 / headroom) * currentOut
        float makeup = (1.0f / headroom) * currentOut;
        juce::FloatVectorOperations::multiply(outData, makeup, numSamples);

        // Safety Brickwall на 0dB (жесткий, чтобы видеть клиппинг в DAW, а не слышать цифровой ад)
        // Или лучше мягкий tanh, как "мастеринг лимитер"
        for (int s=0; s<numSamples; ++s)
        {
             // Мягкий клиппер в самом конце
             outData[s] = std::tanh(outData[s]);
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
