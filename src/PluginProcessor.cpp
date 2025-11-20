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

    smoothedDrive.setCurrentAndTargetValue(1.0f);
    smoothedOutput.setCurrentAndTargetValue(1.0f);
    smoothedMix.setCurrentAndTargetValue(1.0f); // По дефолту Wet, чтобы слышать эффект
    smoothedSatBlend.setCurrentAndTargetValue(0.0f);
    smoothedCompensation.setCurrentAndTargetValue(1.0f);
    smoothedDynamics.setCurrentAndTargetValue(0.5f);

    smoothedNetworkSignal.reset(sampleRate, 0.02);

    // 5. Настройка новых модулей
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

    // --- 2. СЕТЬ (Пока заглушка для стабильности звука) ---

    currentGroup = (int)*apvts.getRawParameterValue("group_id");
    isReference  = (*apvts.getRawParameterValue("role") > 0.5f);
    
    if (isReference) {
        // Шлем пик (просто чтобы видеть, что сеть жива, на звук не влияет пока)
        float maxPeak = buffer.getMagnitude(0, numSamples);
        NetworkManager::getInstance().updateGroupSignal(currentGroup, maxPeak);
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

    // ФИЗИЧЕСКАЯ КОМПЕНСАЦИЯ СУММЫ:
    // Сумма 6 некоррелированных полос дает прирост энергии.
    // Гасим сумму на -9dB (0.35), чтобы вернуть к уровню входа.
    // Плюс простая авто-компенсация драйва (1/drive).
    const float baseSumCompensation = 0.35f; 

    auto* dryL = dryBuffer.getReadPointer(0);
    auto* dryR = (numCh > 1) ? dryBuffer.getReadPointer(1) : nullptr;
    auto* finalL = buffer.getWritePointer(0);
    auto* finalR = (numCh > 1) ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float drv = smoothedDrive.getNextValue();
        float blend = smoothedSatBlend.getNextValue();
        float mix = smoothedMix.getNextValue();
        float outG = smoothedOutput.getNextValue();
        float dynAmount = smoothedDynamics.getNextValue();

        // Predictive Gain (Грубая компенсация внутри полос)
        // Оставляем это, чтобы сумматор не переполнялся в float.
        // Формула: 1/sqrt(drive)
        float predComp = 1.0f / std::sqrt(std::max(1.0f, drv));

        // Если мы в режиме Blend (<15% драйва), отключаем компенсацию, чтобы не менять громкость Dry
        if (blend < 1.0f) predComp = predComp * blend + 1.0f * (1.0f - blend); 

        float sumL = 0.0f;
        float sumR = 0.0f;

        for (int band = 0; band < kNumBands; ++band)
        {
            float bDrive = drv * bandDriveScale[band];

            // Left
            float rawL = bandBuffers[band].getSample(0, i);
            // Сатурируем
            float satL = std::tanh(rawL * bDrive);
            // === DYNAMICS RESTORATION ===
            // Сравниваем чистый rawL и грязный satL -> выравниваем satL
            float restoredL = dynamicsRestorers[band][0].process(rawL, satL, dynAmount);
            sumL += restoredL * predComp;

            // Right
            if (dryR) {
                float rawR = bandBuffers[band].getSample(1, i);
                float satR = std::tanh(rawR * bDrive);
                float restoredR = dynamicsRestorers[band][1].process(rawR, satR, dynAmount);
                sumR += restoredR * predComp;
            }
        }

        // === PSYCHOACOUSTIC AUTO-GAIN ===
        // Скармливаем Dry (эталон) и Wet (грязный). Получаем множитель.
        float dryL_s = dryL[i];
        float dryR_s = dryR ? dryR[i] : 0.0f;

        float psychoScale = psychoGain.processStereoSample(dryL_s, dryR_s, sumL, sumR);

        // Применяем "умный" гейн
        sumL *= psychoScale;
        sumR *= psychoScale;

        // === MIX & OUTPUT ===
        float outL_val = dryL_s * (1.0f - mix) + sumL * mix;
        outL_val *= outG;

        // Soft Clip (Мастеринг-качество)
        // Используем tanh для пиков > 1.0, чтобы не было цифры
        if (std::abs(outL_val) > 1.0f) outL_val = std::tanh(outL_val);
        finalL[i] = outL_val;

        if (finalR) {
            float outR_val = dryR_s * (1.0f - mix) + sumR * mix;
            outR_val *= outG;
            if (std::abs(outR_val) > 1.0f) outR_val = std::tanh(outR_val);
            finalR[i] = outR_val;
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

