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

    // --- 1. Параметры и Сеть ---

    // Обновляем параметры UI
    float driveDb = *apvts.getRawParameterValue("drive_master");
    float outDb   = *apvts.getRawParameterValue("output_gain");

    currentGroup = (int)*apvts.getRawParameterValue("group_id");
    isReference  = (*apvts.getRawParameterValue("role") > 0.5f);

    // Сглаживание ручек
    smoothedDrive.setTargetValue(juce::Decibels::decibelsToGain(driveDb));
    smoothedOutput.setTargetValue(juce::Decibels::decibelsToGain(outDb));

    // Логика Сети (Reference / Listener)
    if (isReference)
    {
        float maxPeak = buffer.getMagnitude(0, numSamples);
        float envValue = envelope.process(maxPeak);
        NetworkManager::getInstance().updateGroupSignal(currentGroup, envValue);
    }

    // Читаем из сети
    float targetNetValue = 0.0f;
    if (!isReference)
    {
        targetNetValue = NetworkManager::getInstance().getGroupSignal(currentGroup);
    }
    // Сглаживаем сетевой сигнал (чтобы не было щелчков при резкой атаке бочки)
    smoothedNetworkSignal.setTargetValue(targetNetValue);

    // Очистка выходов
    for (auto i = totalNumInputChannels; i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, numSamples);

    // --- 2. DSP: Split (Кроссовер) ---
    filterBank->splitIntoBands(buffer, bandBufferPtrs.data(), numSamples);

    // --- 3. DSP: Saturation & Ducking (Посэмпловая обработка) ---

    // Для простоты и скорости, берем параметры 1 раз на блок (кроме сети)
    // (Если нужно супер-плавно крутить ручки, перенесем getNextValue внутрь цикла)
    float baseDrive = smoothedDrive.getNextValue();
    smoothedDrive.skip(numSamples - 1);

    float outGain = smoothedOutput.getNextValue();
    smoothedOutput.skip(numSamples - 1);

    // Флаг "Чистого режима" (если драйв в минимуме)
    bool bypassSaturation = (driveDb < -5.9f);

    for (int i = 0; i < numSamples; ++i)
    {
        // Получаем сглаженное значение сети для этого сэмпла
        float netValue = smoothedNetworkSignal.getNextValue();

        // Рассчитываем "Динамический Драйв"
        // Если Reference (netValue) громкий -> Драйв уменьшается (Ducking)
        // Формула: Drive * (1.0 - Network * Depth)
        float dynamicMod = 1.0f;
        if (!isReference && netValue > 0.0f)
        {
            // Depth пока хардкодом 0.5 (в будущем выведем на ручку)
            dynamicMod = 1.0f - (netValue * 0.5f);
            if (dynamicMod < 0.0f) dynamicMod = 0.0f;
        }

        float currentSampleDrive = baseDrive * dynamicMod;

        // Расчет компенсации для этого сэмпла
        // Чем больше драйв, тем сильнее давим выход полосы
        float bandComp = 1.0f;
        if (currentSampleDrive > 1.0f)
             bandComp = 1.0f / std::pow(currentSampleDrive, 0.6f); // 0.6 - подобранный на слух коэффициент "жира"

        // Обработка всех полос и каналов для этого сэмпла
        for (int band = 0; band < kNumBands; ++band)
        {
            for (int ch = 0; ch < totalNumInputChannels; ++ch)
            {
                // Прямой доступ к сэмплу в буфере полосы
                float* samplePtr = bandBuffers[band].getWritePointer(ch) + i;
                float x = *samplePtr;

                if (!bypassSaturation)
                {
                    // 1. Input Gain
                    x *= currentSampleDrive;

                    // 2. Saturation (Tanh)
                    x = std::tanh(x);

                    // 3. Band Compensation (чтобы не орало при сумме)
                    x *= bandComp;
                }

                *samplePtr = x;
            }
        }
    }

    // --- 4. DSP: Sum & Limit (Сборка) ---
    buffer.clear();

    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto* outData = buffer.getWritePointer(ch);

        for (int band = 0; band < kNumBands; ++band)
        {
            const auto* bandData = bandBuffers[band].getReadPointer(ch);
            juce::FloatVectorOperations::add(outData, bandData, numSamples);
        }

        // Final Output Stage
        for (int i = 0; i < numSamples; ++i)
        {
            float x = outData[i];

            // 1. Ручка Output
            x *= outGain;

            // 2. Safety Limiter (Мягкий клиппер на выходе)
            // Не дает сигналу превысить 0 dBfs (1.0), даже если сумма полос огромная
            // Используем быстрый алгоритм "Hard Clip с скруглением"
            if (x > 1.0f) x = 1.0f;
            else if (x < -1.0f) x = -1.0f;

            outData[i] = x;
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
