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
    smoothedSatBlend.reset(sampleRate, 0.05);

    smoothedDrive.setCurrentAndTargetValue(1.0f);
    smoothedOutput.setCurrentAndTargetValue(1.0f);
    smoothedMix.setCurrentAndTargetValue(1.0f); // По дефолту Wet, чтобы слышать эффект
    smoothedSatBlend.setCurrentAndTargetValue(0.0f);

    smoothedNetworkSignal.reset(sampleRate, 0.02);

    // 5. Настройка новых модулей
    scNormalizer.prepare(sampleRate);

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
    const int numCh = juce::jmin(buffer.getNumChannels(), 2);

    // --- 1. ПАРАМЕТРЫ ---

    float driveParam = *apvts.getRawParameterValue("drive_master");
    float mixParam   = *apvts.getRawParameterValue("mix");
    float outDb      = *apvts.getRawParameterValue("output_gain");

    // Маппинг Drive
    // 0..10 -> Чистая зона (Blend)
    // 10..100 -> Сатурация (Boost)
    float targetSatBlend = juce::jlimit(0.0f, 1.0f, driveParam / 10.0f);
    float driveRest = juce::jmax(0.0f, driveParam - 10.0f);
    float targetDrive = 1.0f + (driveRest * 0.2f); // До +20dB

    smoothedDrive.setTargetValue(targetDrive);
    smoothedSatBlend.setTargetValue(targetSatBlend);
    smoothedMix.setTargetValue(mixParam / 100.0f);
    smoothedOutput.setTargetValue(juce::Decibels::decibelsToGain(outDb));

    // --- 2. СЕТЬ ---

    currentGroup = (int)*apvts.getRawParameterValue("group_id");
    isReference  = (*apvts.getRawParameterValue("role") > 0.5f);
    int modeIndex = *apvts.getRawParameterValue("mode");

    // Детекция для сети (Пиковая, быстрая)
    if (isReference) {
        // Используем Magnitude, это самый быстрый и надежный способ для триггера
        float maxPeak = buffer.getMagnitude(0, numSamples);
        float envValue = envelope.process(maxPeak);
        NetworkManager::getInstance().updateGroupSignal(currentGroup, envValue);
    }

    float rawNetVal = (!isReference) ? NetworkManager::getInstance().getGroupSignal(currentGroup) : 0.0f;
    smoothedNetworkSignal.setTargetValue(rawNetVal);

    // --- 3. DRY COPY ---

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    juce::dsp::AudioBlock<float> dryBlock(dryBuffer);
    juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);
    dryDelayLine.process(dryContext);

    // --- 4. SPLIT ---

    filterBank->splitIntoBands(buffer, bandBufferPtrs.data(), numSamples);

    // Настройки Tilt (меньше искажений на низах)
    // 0(Sub)  1(Low)  2(Mid)  3(MH)  4(High) 5(Air)
    const float bandDriveScale[6] = { 0.5f, 0.8f, 1.0f, 1.0f, 1.1f, 1.2f };

    // === КАЛИБРОВКА ===

    // Если сумма полос дает +12dB, мы давим её на -12dB (0.25x).

    // Это вернет уровень в ноль при Drive=0.

    // Подстрой это число, если будет слишком тихо (например, 0.35f).

    const float wetCalibration = 0.25f;

    // --- 5. PROCESS LOOP ---

    auto* dryL = dryBuffer.getReadPointer(0);
    auto* dryR = (numCh > 1) ? dryBuffer.getReadPointer(1) : nullptr;
    auto* outL = buffer.getWritePointer(0);
    auto* outR = (numCh > 1) ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // Параметры на сэмпл
        float baseDrive = smoothedDrive.getNextValue();
        float blendVal  = smoothedSatBlend.getNextValue();
        float netVal    = smoothedNetworkSignal.getNextValue();
        float mixVal    = smoothedMix.getNextValue();
        float outGain   = smoothedOutput.getNextValue();

        // SC Normalizer (Умный триггер)
        float controlSignal = scNormalizer.process(netVal);

        // Логика режимов
        float effectiveDrive = baseDrive;
        float ghostAmount = 0.0f;

        if (!isReference)
        {
            if (modeIndex == 0) { // Ducking
                effectiveDrive *= (1.0f - controlSignal * 0.8f);
            }
            else if (modeIndex == 1) { // Ghost
                ghostAmount = controlSignal;
                // В Ghost режиме мы сильно разгоняем драйв на ударе
                effectiveDrive *= (1.0f + ghostAmount * 2.0f);
            }
        }

        // Сумматор Wet
        float wetSampleL = 0.0f;
        float wetSampleR = 0.0f;

        for (int band = 0; band < kNumBands; ++band)
        {
            // 1. Локальный драйв полосы
            float bandDrive = effectiveDrive * bandDriveScale[band];

            // 2. Тип сатурации
            SaturationType type = SaturationType::WarmTube;
            float shaperMix = blendVal;

            if (ghostAmount > 0.01f && band <= 1) { // Только Sub и Low
                type = SaturationType::Rectifier;
                shaperMix = std::max(blendVal, ghostAmount);
            }

            // 3. PREDICTIVE AUTO-GAIN (Математическая компенсация)
            // Tanh увеличивает энергию. Мы компенсируем это ПРЕВЕНТИВНО.
            // Чем больше драйв, тем тише выход полосы.
            // Формула 1.0 / sqrt(D) — это "Constant Power Law".
            // Добавляем защиту max(1.0), чтобы не делить на 0 и не глушить чистый звук.
            float comp = 1.0f / std::sqrt(std::max(1.0f, bandDrive));

            // Для Ghost режима (Rectifier) нужна отдельная компенсация, он звучит тоньше
            if (type == SaturationType::Rectifier) comp *= 1.2f;

            // --- Обработка ---

            // Left
            float xL = bandBuffers[band].getSample(0, i);
            float processedL = shapers[band].processSample(xL, bandDrive, type, shaperMix);

            // HARD CLAMP FOR SUB (Защита низа)
            // Если это саб-бас (0), мы не даем ему прыгать выше входного уровня более чем на 10%
            if (band == 0) {
                float maxAmp = std::abs(xL) * 1.1f;
                if (std::abs(processedL * comp) > maxAmp) comp = maxAmp / (std::abs(processedL) + 0.0001f);
            }

            wetSampleL += processedL * comp;

            // Right
            if (outR) {
                float xR = bandBuffers[band].getSample(1, i);
                float processedR = shapers[band].processSample(xR, bandDrive, type, shaperMix);

                if (band == 0) { // Симметричный кламп для правого канала
                    float maxAmp = std::abs(xR) * 1.1f;
                    if (std::abs(processedR * comp) > maxAmp) comp = maxAmp / (std::abs(processedR) + 0.0001f);
                }

                wetSampleR += processedR * comp;
            }
        }

        // Применяем Калибровку к Wet сумме
        wetSampleL *= wetCalibration;
        if (outR) wetSampleR *= wetCalibration;

        // MIX & OUT
        // Если Mix=100%, мы слышим (SaturatedSum * Calibration).
        // При Drive=0 это должно быть равно Input.
        float finalL = dryL[i] * (1.0f - mixVal) + wetSampleL * mixVal;
        outL[i] = std::tanh(finalL * outGain); // Soft Clipper на мастере

        if (outR) {
            float finalR = dryR[i] * (1.0f - mixVal) + wetSampleR * mixVal;
            outR[i] = std::tanh(finalR * outGain);
        }
    }

    // Очистка
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
