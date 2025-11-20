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

    // Глобальный Drive (для примера)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "drive_master", "Master Drive",
        juce::NormalisableRange<float>(0.0f, 24.0f, 0.1f), 0.0f));

    // Output Gain
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "output_gain", "Output Gain",
        juce::NormalisableRange<float>(-24.0f, 24.0f, 0.1f), 0.0f));

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

    // Настраиваем сглаживатели
    // Время сглаживания: 0.02 сек (20 мс) - достаточно быстро, но плавно
    smoothedDrive.reset(sampleRate, 0.02);
    smoothedOutput.reset(sampleRate, 0.02);

    // Начальные значения
    smoothedDrive.setCurrentAndTargetValue(1.0f); // Drive = 1.0 (нет искажений)
    smoothedOutput.setCurrentAndTargetValue(1.0f);
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
    const int totalNumOutputChannels = getTotalNumOutputChannels();
    const int numSamples = buffer.getNumSamples();

    // Очистка лишних каналов
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, numSamples);

    // --- 1. Читаем параметры (APVTS) ---

    // Получаем значение в Децибелах из слайдера (0..24 dB)
    float driveDb = *apvts.getRawParameterValue("drive_master");
    float outDb   = *apvts.getRawParameterValue("output_gain");

    // Конвертируем dB в линейное усиление (Gain)
    float targetDrive = juce::Decibels::decibelsToGain(driveDb);
    float targetOut   = juce::Decibels::decibelsToGain(outDb);

    // Обновляем сглаживатели
    smoothedDrive.setTargetValue(targetDrive);
    smoothedOutput.setTargetValue(targetOut);

    // --- 2. DSP: Split (Кроссовер) ---
    filterBank->splitIntoBands(buffer, bandBufferPtrs.data(), numSamples);

    // --- 3. DSP: Saturation (Сатурация полос) ---
    // Мы обрабатываем каждую полосу отдельно!

    // Для оптимизации: получим текущее значение drive один раз на блок
    // (или можно делать smoothedDrive.getNextValue() внутри цикла для супер-плавности)
    float currentDrive = smoothedDrive.getNextValue();
    float currentOut   = smoothedOutput.getNextValue();

    for (int band = 0; band < kNumBands; ++band)
    {
        // Проходим по каналам (L/R)
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            auto* channelData = bandBuffers[band].getWritePointer(ch);

            for (int i = 0; i < numSamples; ++i)
            {
                // Берем сэмпл
                float inSample = channelData[i];

                // Сатурируем!
                // Используем WarmTube (tanh) для начала
                float processed = shapers[band].processSample(inSample, currentDrive, SaturationType::WarmTube);

                // Записываем обратно
                channelData[i] = processed;
            }
        }
    }

    // --- 4. DSP: Sum (Сборка) ---
    buffer.clear(); // Очищаем выход перед суммированием

    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto* outData = buffer.getWritePointer(ch);

        for (int band = 0; band < kNumBands; ++band)
        {
            const auto* bandData = bandBuffers[band].getReadPointer(ch);
            juce::FloatVectorOperations::add(outData, bandData, numSamples);
        }

        // Применяем Output Gain ко всему миксу
        juce::FloatVectorOperations::multiply(outData, currentOut, numSamples);
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
