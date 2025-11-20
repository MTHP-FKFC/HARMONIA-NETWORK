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

    // Режим работы: 3 режима взаимодействия
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "mode", "Interaction Mode",
        juce::StringArray{
            "Unmasking (Duck)",   // 0: Реф громкий -> Мы чище
            "Ghost (Follow)",     // 1: Реф громкий -> Мы злее
            "Gated (Reverse)"     // 2: Реф тихий -> Мы злее
        }, 0));

    // Dynamics Preservation
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "dynamics", "Dynamics Preservation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));

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
    smoothedCompensation.reset(sampleRate, 0.1);
    smoothedDynamics.reset(sampleRate, 0.05);

    // Инициализация Dynamics Restorers
    for (int b = 0; b < kNumBands; ++b) {
        for (int ch = 0; ch < 2; ++ch) {
            dynamicsRestorers[b][ch].prepare(sampleRate);
        }
    }

    psychoGain.prepare(sampleRate); // <-- NEW

    // Инициализация полосных энвелопов и сглаживателей сети
    for (int i = 0; i < kNumBands; ++i)
    {
        // Настраиваем детекторы (быстрые)
        bandEnvelopes[i].reset(sampleRate);

        // Настраиваем сглаживатели приема (20ms - достаточно быстро для транзиентов, но без треска)
        smoothedNetworkBands[i].reset(sampleRate, 0.02f);
        smoothedNetworkBands[i].setCurrentAndTargetValue(0.0f);
    }

    // SOFT START:
    // Начинаем с тишины (0.0)
    // Поднимаемся до полной громкости (1.0) за 200 мс.
    // Этого времени хватит, чтобы FIR-фильтры заполнились данными.
    startupFader.reset(sampleRate, 0.2f);
    startupFader.setCurrentAndTargetValue(0.0f);
    startupFader.setTargetValue(1.0f);

    smoothedDrive.setCurrentAndTargetValue(1.0f);
    smoothedOutput.setCurrentAndTargetValue(1.0f);
    smoothedMix.setCurrentAndTargetValue(1.0f); // По дефолту Wet, чтобы слышать эффект
    smoothedSatBlend.setCurrentAndTargetValue(0.0f);
    smoothedCompensation.setCurrentAndTargetValue(1.0f);
    smoothedDynamics.setCurrentAndTargetValue(0.5f);

    smoothedNetworkSignal.reset(sampleRate, 0.02);

    // 5. Настройка новых модулей
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

    float driveParam = *apvts.getRawParameterValue("drive_master"); // 0..100
    float mixParam   = *apvts.getRawParameterValue("mix");          // 0..100
    float outDb      = *apvts.getRawParameterValue("output_gain");

    // Умный маппинг драйва (0..15% blend, 15..100% boost)
    float targetSatBlend = juce::jlimit(0.0f, 1.0f, driveParam / 15.0f);
    float driveRest = juce::jmax(0.0f, driveParam - 15.0f);
    float targetDrive = 1.0f + (driveRest * 0.2f); // Макс x18

    // Читаем параметр Dynamics
    float dynParam = *apvts.getRawParameterValue("dynamics");

    // Сглаживаем параметры
    smoothedDrive.setTargetValue(targetDrive);
    smoothedSatBlend.setTargetValue(targetSatBlend);
    smoothedMix.setTargetValue(mixParam / 100.0f);
    smoothedOutput.setTargetValue(juce::Decibels::decibelsToGain(outDb));
    smoothedDynamics.setTargetValue(dynParam / 100.0f);

    // --- 2. СЕТЕВАЯ ЛОГИКА (Per-Band) ---

    currentGroup = (int)*apvts.getRawParameterValue("group_id");
    isReference  = (*apvts.getRawParameterValue("role") > 0.5f);

    // Читаем параметры режима
    int modeIndex = *apvts.getRawParameterValue("mode");

    // Если Reference -> Анализируем каждую полосу и шлем в сеть
    if (isReference)
    {
        for (int b = 0; b < kNumBands; ++b)
        {
            // Берем пик громкости в этой полосе (из L и R)
            float peakL = bandBuffers[b].getMagnitude(0, 0, numSamples);
            float peakR = (numCh > 1) ? bandBuffers[b].getMagnitude(1, 0, numSamples) : 0.0f;
            float maxBandPeak = std::max(peakL, peakR);

            // Обрабатываем энвелопом
            float envVal = bandEnvelopes[b].process(maxBandPeak);

            // Шлем в сеть
            NetworkManager::getInstance().updateBandSignal(currentGroup, b, envVal);
        }
    }

    // Если Listener -> Читаем каждую полосу из сети и готовим сглаживатели
    for (int b = 0; b < kNumBands; ++b)
    {
        float targetVal = 0.0f;
        if (!isReference) {
            targetVal = NetworkManager::getInstance().getBandSignal(currentGroup, b);
        }
        smoothedNetworkBands[b].setTargetValue(targetVal);
    }

    // --- 3. DRY COPY & SPLIT ---

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);
    juce::dsp::AudioBlock<float> dryBlock(dryBuffer);
    juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);
    dryDelayLine.process(dryContext);

    filterBank->splitIntoBands(buffer, bandBufferPtrs.data(), numSamples);

    // --- 4. PROCESS ---
    
    // Временный буфер для суммы Wet сигнала
    juce::AudioBuffer<float> wetSumBuffer;
    wetSumBuffer.setSize(numCh, numSamples);
    wetSumBuffer.clear();

    // Фиксированный Tilt (меньше грязи на низах)
    const float bandDriveScale[6] = { 0.6f, 0.8f, 1.0f, 1.0f, 1.1f, 1.2f };


    auto* dryL = dryBuffer.getReadPointer(0);
    auto* dryR = (numCh > 1) ? dryBuffer.getReadPointer(1) : nullptr;
    auto* finalL = buffer.getWritePointer(0);
    auto* finalR = (numCh > 1) ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        // Базовые параметры
        float baseDrive = smoothedDrive.getNextValue();
        float blendVal  = smoothedSatBlend.getNextValue();
        float mixVal    = smoothedMix.getNextValue();
        float outG      = smoothedOutput.getNextValue();

        float wetSampleL = 0.0f;
        float wetSampleR = 0.0f;

        for (int band = 0; band < kNumBands; ++band)
        {
            // 1. Получаем сигнал сети для ЭТОЙ полосы
            float bandNetVal = smoothedNetworkBands[band].getNextValue();

            // Гейт шума, чтобы не триггерилось на тишину
            float controlSignal = bandNetVal;
            if (controlSignal < 0.02f) controlSignal = 0.0f;

            // 2. ЛОГИКА ВЗАИМОДЕЙСТВИЯ (Per-Band!)
            float effectiveBandDrive = baseDrive * bandDriveScale[band];
            SaturationType type = SaturationType::WarmTube;
            float shaperMix = blendVal;

            if (!isReference)
            {
                if (modeIndex == 0) // MODE: UNMASKING (Ducking)
                {
                    // Реф громкий -> Мы тише/чище
                    if (controlSignal > 0.0f) {
                        float duck = 1.0f - (controlSignal * 0.8f);
                        effectiveBandDrive *= duck;
                    }
                }
                else if (modeIndex == 1) // MODE: GHOST (Direct Follower)
                {
                    // Реф громкий -> Мы злее/ярче
                    if (band <= 2 && controlSignal > 0.0f) { // Только низы
                        type = SaturationType::Rectifier;
                        shaperMix = std::max(blendVal, controlSignal);
                        effectiveBandDrive *= (1.0f + controlSignal * 2.0f);
                    }
                }
                else if (modeIndex == 2) // MODE: GATED (Reverse)
                {
                    // Реф тихий -> Мы злее
                    // Реф громкий -> Мы чище
                    float reverseSignal = 1.0f - controlSignal;

                    if (band <= 2 && reverseSignal > 0.0f) {
                        type = SaturationType::Rectifier;
                        shaperMix = std::max(blendVal, reverseSignal);
                        effectiveBandDrive *= (1.0f + reverseSignal * 1.5f);
                    }
                }
            }

            // 3. Predictive Gain (с учетом измененного драйва)
            float predComp = 1.0f / std::sqrt(std::max(1.0f, effectiveBandDrive));
            if (type == SaturationType::Rectifier) predComp *= 1.2f;

            // 4. Обработка
            // Left
            float rawL = bandBuffers[band].getSample(0, i);
            float satL = shapers[band].processSample(rawL, effectiveBandDrive, type, shaperMix);
            wetSampleL += satL * predComp;

            // Right
            if (dryR) {
                float rawR = bandBuffers[band].getSample(1, i);
                float satR = shapers[band].processSample(rawR, effectiveBandDrive, type, shaperMix);
                wetSampleR += satR * predComp;
            }
        }

        // === PSYCHOACOUSTIC AUTO-GAIN ===
        // Скармливаем Dry (эталон) и Wet (грязный). Получаем множитель.
        float dryL_s = dryL[i];
        float dryR_s = dryR ? dryR[i] : 0.0f;

        float psychoScale = psychoGain.processStereoSample(dryL_s, dryR_s, wetSampleL, wetSampleR);

        // Применяем "умный" гейн
        wetSampleL *= psychoScale;
        wetSampleR *= psychoScale;

        // === MIX & OUTPUT ===
        float outL_val = dryL_s * (1.0f - mixVal) + wetSampleL * mixVal;
        outL_val *= outG;

        // Soft Clip (Мастеринг-качество)
        // Используем tanh для пиков > 1.0, чтобы не было цифры
        if (std::abs(outL_val) > 1.0f) outL_val = std::tanh(outL_val);
        finalL[i] = outL_val;

        if (finalR) {
            float outR_val = dryR_s * (1.0f - mixVal) + wetSampleR * mixVal;
            outR_val *= outG;
            if (std::abs(outR_val) > 1.0f) outR_val = std::tanh(outR_val);
            finalR[i] = outR_val;
        }

        // === SOFT START FADE ===
        // Получаем множитель (от 0.0 до 1.0)
        // После первых 200мс он всегда будет равен 1.0 и перестанет влиять на звук.
        float fade = startupFader.getNextValue();

        finalL[i] *= fade;
        if (finalR) finalR[i] *= fade;
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

