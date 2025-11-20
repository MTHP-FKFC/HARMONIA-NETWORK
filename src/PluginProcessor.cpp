#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "dsp/Waveshaper.h"
#include "dsp/InteractionEngine.h"
#include "dsp/MSMatrix.h"


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

    // Режим работы: 5 режимов взаимодействия
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "mode", "Interaction Mode",
        juce::StringArray{
            "Unmasking (Duck)",   // 0: Реф громкий -> Мы чище
            "Ghost (Follow)",     // 1: Реф громкий -> Мы злее
            "Gated (Reverse)",    // 2: Реф тихий -> Мы злее
            "Stereo Bloom",       // 3: Реф громкий -> Мы расширяемся
            "Sympathetic"         // 4: Реф частоты -> Мы резонируем
        }, 0));

    // Тип сатурации (базовый выбор пользователя)
    layout.add(std::make_unique<juce::AudioParameterChoice>(
        "sat_type", "Saturation Type",
        juce::StringArray{
            "Warm Tube",    // 0: Tanh
            "Asymmetric",   // 1: Even Harmonics
            "Hard Clip",    // 2: Brickwall
            "Bit Crush"     // 3: Digital
        }, 0));

    // Dynamics Preservation
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "dynamics", "Dynamics Preservation",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 50.0f));

    // === NETWORK CONTROL (The Holy Trinity) ===

    // 1. Depth: Насколько сильно мы реагируем (0% = игнорируем сеть, 100% = полный морфинг)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "net_depth", "Interaction Depth",
        juce::NormalisableRange<float>(0.0f, 100.0f, 1.0f), 100.0f)); // Default 100%

    // 2. Smooth: Время реакции (0ms = мгновенная, 200ms = ленивая)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "net_smooth", "Reaction Smooth",
        juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f), 10.0f));  // Default 10ms

    // 3. Sensitivity: Чувствительность (0% = не реагируем, 200% = гипер-чувствительный)
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        "net_sens", "Sensitivity",
        juce::NormalisableRange<float>(0.0f, 200.0f, 1.0f), 100.0f)); // Default 100%

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

    // === Network Control (The Holy Trinity) ===
    smoothedNetDepth.reset(sampleRate, 0.05);
    smoothedNetDepth.setCurrentAndTargetValue(1.0f); // 100% depth

    smoothedNetSens.reset(sampleRate, 0.05);
    smoothedNetSens.setCurrentAndTargetValue(1.0f); // 100% sensitivity

    netSmoothState = 0.0f; // Reset One-Pole filter state

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

    // === NETWORK CONTROL (The Holy Trinity) ===
    float pDepth = *apvts.getRawParameterValue("net_depth");
    float pSmooth = *apvts.getRawParameterValue("net_smooth");
    float pSens  = *apvts.getRawParameterValue("net_sens");

    // Сглаживаем параметры
    smoothedDrive.setTargetValue(targetDrive);
    smoothedSatBlend.setTargetValue(targetSatBlend);
    smoothedMix.setTargetValue(mixParam / 100.0f);
    smoothedOutput.setTargetValue(juce::Decibels::decibelsToGain(outDb));
    smoothedDynamics.setTargetValue(dynParam / 100.0f);

    // Network Control сглаживание
    smoothedNetDepth.setTargetValue(pDepth / 100.0f);
    smoothedNetSens.setTargetValue(pSens / 100.0f);

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

    // === PREPARE ONE-POLE FILTER COEFFICIENT FOR SMOOTH ===
    // Вычисляем коэффициент сглаживания (экспоненциальный фильтр)
    float smoothCoeff = 0.0f;
    if (pSmooth > 0.0f) {
        smoothCoeff = std::exp(-1.0f / (std::max(1.0f, pSmooth) * 0.001f * (float)getSampleRate()));
    }

    for (int i = 0; i < numSamples; ++i)
    {
        // === NETWORK CONTROL PROCESSING (The Holy Trinity) ===
        float depth = smoothedNetDepth.getNextValue();
        float sens  = smoothedNetSens.getNextValue();

        // Базовые параметры
        float baseDrive = smoothedDrive.getNextValue();
        float blendVal  = smoothedSatBlend.getNextValue();
        float mixVal    = smoothedMix.getNextValue();
        float outG      = smoothedOutput.getNextValue();

        float wetSampleL = 0.0f;
        float wetSampleR = 0.0f;

        // Читаем выбор пользователя (базовый тип сатурации)
        int userSatTypeIdx = *apvts.getRawParameterValue("sat_type");
        SaturationType userType = static_cast<SaturationType>(userSatTypeIdx + 1); // +1 т.к. Clean=0 мы не дали в UI

        for (int band = 0; band < kNumBands; ++band)
        {
            // 1. Получаем "сырой" сигнал сети для этой полосы
            float bandNetVal = smoothedNetworkBands[band].getNextValue();

            // 2. SENSITIVITY: Усиливает входящий сигнал ДО нормализации
            float rawNetSignal = bandNetVal * sens;

            // 3. NORMALIZATION: Простой гейт (замена SidechainNormalizer)
            float normalizedSignal = rawNetSignal;
            if (normalizedSignal < 0.05f) normalizedSignal = 0.0f; // Умный гейт

            // 4. SMOOTH: One-Pole фильтр для инерции
            if (pSmooth > 0.0f) {
                netSmoothState = normalizedSignal * (1.0f - smoothCoeff) + netSmoothState * smoothCoeff;
            } else {
                netSmoothState = normalizedSignal; // Без сглаживания
            }

            // 5. DEPTH: Масштабируем итоговый контрольный сигнал
            float controlSignal = netSmoothState * depth;

            // 6. CLAMP: Защита от выхода за границы
            controlSignal = juce::jlimit(0.0f, 1.0f, controlSignal);

            // 2. Получаем конфигурацию для этой полосы
            auto config = InteractionEngine::getConfiguration(modeIndex, band, userType);

            // 3. Логика микса для Stereo Bloom (M/S)
            bool useMS = false;
            float mid = 0.0f, side = 0.0f;
            float xL = bandBuffers[band].getSample(0, i);
            float xR = (numCh > 1) ? bandBuffers[band].getSample(1, i) : xL;

            if (modeIndex == 3 && !isReference && controlSignal > 0.0f)
            {
                useMS = true;
                MSMatrix::encode(xL, xR, mid, side);
            }

            // 4. Обработка через InteractionEngine
            float procL = 0.0f, procR = 0.0f;

            if (useMS)
            {
                // M/S: Mid через A, Side через B
                float pMid = InteractionEngine::processMorph(mid, baseDrive * bandDriveScale[band], controlSignal, config);
                float pSide = InteractionEngine::processMorph(side, baseDrive * bandDriveScale[band], controlSignal, config);
                MSMatrix::decode(pMid, pSide, procL, procR);
            }
            else
            {
                // Обычная L/R обработка
                procL = InteractionEngine::processMorph(xL, baseDrive * bandDriveScale[band], controlSignal, config);
                if (numCh > 1)
                    procR = InteractionEngine::processMorph(xR, baseDrive * bandDriveScale[band], controlSignal, config);
            }

            // 5. Predictive Gain
            float avgScale = config.driveScaleA * (1.0f - controlSignal) + config.driveScaleB * controlSignal;
            float predComp = 1.0f / std::sqrt(std::max(1.0f, baseDrive * bandDriveScale[band] * avgScale));
            if (config.typeA == SaturationType::Rectifier || config.typeB == SaturationType::Rectifier)
                predComp *= 1.2f;

            wetSampleL += procL * predComp;
            if (numCh > 1) wetSampleR += procR * predComp;
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

