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

    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "mix", "Mix",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f)); // Дефолт 50%

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

    // 4. Сглаживание и Энвелопы
    smoothedDrive.reset(sampleRate, 0.05);
    smoothedOutput.reset(sampleRate, 0.05);
    smoothedMix.reset(sampleRate, 0.05);
    smoothedNetworkSignal.reset(sampleRate, 0.01);

    envelope.reset(sampleRate);       // Input env
    outputEnvelope.reset(sampleRate); // Output env

    // Начальные значения
    smoothedDrive.setCurrentAndTargetValue(1.0f);
    smoothedOutput.setCurrentAndTargetValue(1.0f);
    smoothedMix.setCurrentAndTargetValue(0.5f); // 50%
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

    // --- 1. Подготовка ---

    float driveDb = *apvts.getRawParameterValue("drive_master");
    float outDb   = *apvts.getRawParameterValue("output_gain");
    float mixPerc = *apvts.getRawParameterValue("mix");

    currentGroup = (int)*apvts.getRawParameterValue("group_id");
    isReference  = (*apvts.getRawParameterValue("role") > 0.5f);

    smoothedDrive.setTargetValue(juce::Decibels::decibelsToGain(driveDb));
    smoothedOutput.setTargetValue(juce::Decibels::decibelsToGain(outDb));
    smoothedMix.setTargetValue(mixPerc / 100.0f);

    // Сеть
    float inMag = buffer.getMagnitude(0, numSamples);
    float inEnv = envelope.process(inMag);
    if (isReference) NetworkManager::getInstance().updateGroupSignal(currentGroup, inEnv);

    float netVal = isReference ? 0.0f : NetworkManager::getInstance().getGroupSignal(currentGroup);
    smoothedNetworkSignal.setTargetValue(netVal);

    // --- 2. Сохраняем DRY сигнал (с задержкой!) ---

    // Копируем вход в dryBuffer
    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer); // Чистый вход без задержки

    // Применяем Delay к dryBuffer
    juce::dsp::AudioBlock<float> dryBlockDelayed(dryBuffer);
    juce::dsp::ProcessContextReplacing<float> dryContextDelayed(dryBlockDelayed);
    dryDelayLine.process(dryContextDelayed); // Теперь dryBuffer задержан на латентность фильтров

    // --- 3. DSP: Split (Wet Path) ---
    filterBank->splitIntoBands(buffer, bandBufferPtrs.data(), numSamples);

    // --- 4. DSP: Saturation ---

    float currentDrive = smoothedDrive.getNextValue(); smoothedDrive.skip(numSamples-1);
    float currentMix   = smoothedMix.getNextValue();   smoothedMix.skip(numSamples-1);
    float currentOut   = smoothedOutput.getNextValue(); smoothedOutput.skip(numSamples-1);

    // Авто-гейн на основе физики (приблизительный, но быстрый)
    // Tanh "съедает" энергию. Мы компенсируем её.
    float makeUp = 1.0f;
    if (currentDrive > 1.0f) makeUp = std::sqrt(currentDrive); // Возвращаем энергию

    for (int i = 0; i < numSamples; ++i)
    {
        float net = smoothedNetworkSignal.getNextValue();

        // Ducking (Inverse Grime)
        float driveMod = 1.0f;
        if (!isReference && net > 0.0f) driveMod = 1.0f - (net * 0.6f); // 60% ducking

        float effectiveDrive = currentDrive * driveMod;

        for (int band = 0; band < kNumBands; ++band)
        {
            for (int ch = 0; ch < 2; ++ch)
            {
                float* s = bandBuffers[band].getWritePointer(ch) + i;

                // Saturation
                float x = *s * effectiveDrive;

                // Soft Clip (Tanh)
                x = std::tanh(x);

                // Makeup Gain (возвращаем громкость после компрессии tanh)
                x *= makeUp;

                *s = x;
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

        // Output Gain
        juce::FloatVectorOperations::multiply(outData, currentOut, numSamples);

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
