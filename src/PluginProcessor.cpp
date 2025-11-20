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
    smoothedCompensation.reset(sampleRate, 0.1);

    smoothedDrive.setCurrentAndTargetValue(1.0f);
    smoothedOutput.setCurrentAndTargetValue(1.0f);
    smoothedMix.setCurrentAndTargetValue(1.0f); // По дефолту Wet, чтобы слышать эффект
    smoothedSatBlend.setCurrentAndTargetValue(0.0f);
    smoothedCompensation.setCurrentAndTargetValue(1.0f);

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

    // ЛОГИКА "ЧИСТОГО НУЛЯ":
    // Если Drive < 20%, мы плавно переходим от Чистого сигнала к Сатурированному.
    // Если Drive > 20%, мы начинаем наваливать Гейн.
    float driveInputGain = 1.0f;
    float cleanToSatMix = 0.0f;

    if (driveParam < 20.0f)
    {
        // Режим "Начало": плавно вводим сатурацию
        // 0% -> mix 0.0 (чистый)
        // 20% -> mix 1.0 (полностью в tanh)
        driveInputGain = 1.0f;
        cleanToSatMix = driveParam / 20.0f;
    }
    else
    {
        // Режим "Жар": разгоняем вход
        // 20% -> Gain 1.0
        // 100% -> Gain 10.0 (+20dB)
        cleanToSatMix = 1.0f;
        float boost = (driveParam - 20.0f) / 80.0f; // 0..1
        driveInputGain = 1.0f + (boost * 9.0f); 
    }

    // Сглаживаем параметры
    smoothedDrive.setTargetValue(driveInputGain);
    smoothedSatBlend.setTargetValue(cleanToSatMix); // Используем эту переменную для кроссфейда Clean/Sat
    smoothedMix.setTargetValue(mixParam / 100.0f);
    smoothedOutput.setTargetValue(juce::Decibels::decibelsToGain(outDb));

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
    const float bandDriveScale[6] = { 0.5f, 0.8f, 1.0f, 1.0f, 1.1f, 1.2f };

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

        // Компенсация драйва: чем больше навалили на вход, тем тише делаем выход
        // (чтобы громкость не менялась при вращении ручки)
        float driveComp = 1.0f / std::sqrt(drv); 

        float sumL = 0.0f;
        float sumR = 0.0f;

        for (int band = 0; band < kNumBands; ++band)
        {
            float bDrive = drv * bandDriveScale[band];
            
            // Left
            float rawL = bandBuffers[band].getSample(0, i);
            // Если blend < 1.0, мы подмешиваем чистый сигнал к сатурированному
            // Blend=0 -> Tanh не работает, звук чистый
            float satL = std::tanh(rawL * bDrive);
            float resL = rawL + blend * (satL - rawL);
            sumL += resL;

            // Right
            if (dryR) {
                float rawR = bandBuffers[band].getSample(1, i);
                float satR = std::tanh(rawR * bDrive);
                float resR = rawR + blend * (satR - rawR);
                sumR += resR;
            }
        }

        // Применяем все компенсации к Wet-сумме
        // 1. baseSumCompensation - гасит прирост от сложения полос
        // 2. driveComp - гасит прирост от ручки Drive
        // 3. (1.0/0.35) * (1-blend) - хак: если мы в чистом режиме (blend=0), 
        //    мы должны ОТМЕНИТЬ baseSumCompensation, иначе чистый звук будет тихим.
        
        float totalWetComp = baseSumCompensation * driveComp;
        
        // Восстановление громкости на малых значениях Drive (когда blend < 1)
        // Чтобы на 0% Drive громкость была равна входной
        if (blend < 1.0f) {
            totalWetComp = totalWetComp * blend + 1.0f * (1.0f - blend);
        }

        sumL *= totalWetComp;
        sumR *= totalWetComp;

        // MIX (Linear)
        float outL_val = dryL[i] * (1.0f - mix) + sumL * mix;
        float outR_val = (dryR) ? (dryR[i] * (1.0f - mix) + sumR * mix) : 0.0f;

        // OUTPUT GAIN
        outL_val *= outG;
        outR_val *= outG;

        // BRICKWALL LIMITER (Safety)
        // Жестко режем всё, что выше -0.1dB, чтобы не было клиппинга в DAW
        outL_val = std::max(-0.99f, std::min(0.99f, outL_val));
        if (dryR) outR_val = std::max(-0.99f, std::min(0.99f, outR_val));

        finalL[i] = outL_val;
        if (finalR) finalR[i] = outR_val;
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
