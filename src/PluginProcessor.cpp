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

    smoothedDrive.setCurrentAndTargetValue(1.0f);
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
    const int numSamples = buffer.getNumSamples();

    // --- 1. Параметры ---

    // Расширим диапазон вниз, чтобы можно было сделать чистый звук
    // Если в Editor'е слайдер от 0, то считаем, что это "добавочный" гейн.
    // Но давай пока считать так: параметр APVTS - это dB.
    float driveDb = *apvts.getRawParameterValue("drive_master");
    float outDb   = *apvts.getRawParameterValue("output_gain");

    float targetDrive = juce::Decibels::decibelsToGain(driveDb);
    float targetOut   = juce::Decibels::decibelsToGain(outDb);

    smoothedDrive.setTargetValue(targetDrive);
    smoothedOutput.setTargetValue(targetOut);

    // FIX: Получаем текущее значение и "проматываем" сглаживатель на длину блока,
    // так как мы не вызываем getNextValue() для каждого сэмпла (экономим CPU).
    float currentDrive = smoothedDrive.getNextValue();
    smoothedDrive.skip(numSamples - 1);

    float currentOut = smoothedOutput.getNextValue();
    smoothedOutput.skip(numSamples - 1);

    // === НОВАЯ ЛОГИКА КОМПЕНСАЦИИ ===
    // Если мы наваливаем драйв, мы хотим компенсировать выходную громкость.
    // Используем корень квадратный: это сохраняет энергию, но убирает пики.
    // Если Drive = 4.0, мы умножим выход на 0.5.
    float compensation = 1.0f;
    if (currentDrive > 1.0f)
    {
        compensation = 1.0f / std::sqrt(currentDrive);
    }

    // Очистка мусора
    for (auto i = totalNumInputChannels; i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, numSamples);

    // --- 2. DSP: Split ---
    filterBank->splitIntoBands(buffer, bandBufferPtrs.data(), numSamples);

    // --- 3. DSP: Saturation ---
    // FIX: Если Drive очень маленький, просто пропускаем звук (Bypass сатурации)
    // Это поможет проверить качество фильтров.
    bool bypassSaturation = (driveDb < 0.1f);

    for (int band = 0; band < kNumBands; ++band)
    {
        for (int ch = 0; ch < totalNumInputChannels; ++ch)
        {
            auto* channelData = bandBuffers[band].getWritePointer(ch);

            if (!bypassSaturation)
            {
                for (int i = 0; i < numSamples; ++i)
                {
                    // Tanh (Soft Clip)
                    // Убрали inSample * currentDrive, чтобы не бустить громкость дико.
                    // Tanh сам по себе ограничивает сигнал в 1.0.
                    // Drive здесь работает как "накачка" перед Tanh.
                    float x = channelData[i] * currentDrive;
                    channelData[i] = std::tanh(x);
                }
            }
            // Если bypassSaturation == true, буфер остается нетронутым (чистый выход фильтра)
        }
    }

    // --- 4. DSP: Sum & Apply Compensation ---
    buffer.clear();

    for (int ch = 0; ch < totalNumInputChannels; ++ch)
    {
        auto* outData = buffer.getWritePointer(ch);

        for (int band = 0; band < kNumBands; ++band)
        {
            const auto* bandData = bandBuffers[band].getReadPointer(ch);
            juce::FloatVectorOperations::add(outData, bandData, numSamples);
        }

        // ПРИМЕНЯЕМ КОМПЕНСАЦИЮ + OUTPUT GAIN
        // Сначала компенсируем перегруз от драйва, потом применяем ручку Output
        float finalGain = currentOut * compensation;

        juce::FloatVectorOperations::multiply(outData, finalGain, numSamples);

        // Safety Limiter (мягкий клиппер вместо жесткого clamp)
        for (int i=0; i<numSamples; ++i)
        {
            // Мягкий клиппер на выходе: tanh как мастер-лимитер
            outData[i] = std::tanh(outData[i]);
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
