#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/Waveshaper.h"

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

    // Режим работы: 0 = Inverse Grime, 1 = Ghost Harmonics
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "mode", "Interaction Mode",
        juce::StringArray{"Inverse Grime", "Ghost Harmonics"}, 0));

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

    // 5. Настройка новых модулей
    scNormalizer.prepare(sampleRate);
    autoGain.prepare(sampleRate);

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

    // --- 2. ANALYZE INPUT & NETWORK ---

    autoGain.analyzeInput(buffer);

    currentGroup = (int)*apvts.getRawParameterValue("group_id");
    isReference  = (*apvts.getRawParameterValue("role") > 0.5f);

    if (isReference) {
        // Гейт: если бочка тише -40dB, шлем ноль.
        float maxPeak = buffer.getMagnitude(0, numSamples);
        float gatedLevel = maxPeak;
        if (maxPeak < 0.01f) gatedLevel = 0.0f;

        NetworkManager::getInstance().updateGroupSignal(currentGroup, gatedLevel);
    }

    float rawNetVal = (!isReference) ? NetworkManager::getInstance().getGroupSignal(currentGroup) : 0.0f;
    smoothedNetworkSignal.setTargetValue(rawNetVal);

    // Читаем какой режим выбрал юзер
    int modeIndex = *apvts.getRawParameterValue("mode"); // 0 или 1

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

    for (int i = 0; i < numSamples; ++i)
    {
        // А. Получаем "Умный" управляющий сигнал
        // Вся логика адаптивного порога теперь внутри класса!
        float rawNetSample = smoothedNetworkSignal.getNextValue();
        float controlSignal = scNormalizer.process(rawNetSample);

        // Б. Рассчитываем параметры для этого сэмпла
        float driveVal = smoothedDrive.getNextValue();
        float compVal  = autoGain.getNextValue();

        // Логика режимов теперь читается легко:
        float effectiveDrive = driveVal;
        float ghostAmount = 0.0f;

        if (!isReference)
        {
            if (modeIndex == 0) { // Ducking
                effectiveDrive *= (1.0f - controlSignal);
            }
            else if (modeIndex == 1) { // Ghost
                ghostAmount = controlSignal;
                effectiveDrive *= (1.0f + ghostAmount * 1.5f);
            }
        }

        // В. Сатурация полос
        float wetSampleL = 0.0f;
        float wetSampleR = 0.0f;

        for (int band = 0; band < kNumBands; ++band)
        {
            for (int ch = 0; ch < numCh; ++ch)
            {
                float x = bandBuffers[band].getSample(ch, i);
                float drivenX = x * effectiveDrive;
                float processed = 0.0f;

                if (modeIndex == 1 && !isReference && band <= 2)
                {
                    float cleanTube = std::tanh(drivenX);
                    float fold = std::sin(drivenX * 1.5f);
                    processed = cleanTube * (1.0f - ghostAmount) + fold * ghostAmount;
                }
                else
                {
                    processed = std::tanh(drivenX);
                }

                if (ch == 0) wetSampleL += processed;
                else         wetSampleR += processed;
            }
        }

        // Г. Применение автогейна к сэмплу (ДО микширования)
        wetSampleL *= compVal;
        if (outR) wetSampleR *= compVal;

        // Д. Mix & Output
        float mixVal = smoothedMix.getNextValue();
        float outGain = smoothedOutput.getNextValue();

        float finalL = dryL[i] * (1.0f - mixVal) + wetSampleL * mixVal;
        finalL *= outGain;
        finalL = std::tanh(finalL);
        outL[i] = finalL;

        if (outR) {
            float finalR = dryR[i] * (1.0f - mixVal) + wetSampleR * mixVal;
            finalR *= outGain;
            finalR = std::tanh(finalR);
            outR[i] = finalR;
        }
    }

    // 5. UPDATE AUTO-GAIN STATE
    // Скармливаем результат в авто-гейн, чтобы он подготовился к следующему блоку
    autoGain.updateGainState(buffer);

    // Очистка мусора в неиспользуемых каналах
    for (int i = numCh; i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, numSamples);

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
