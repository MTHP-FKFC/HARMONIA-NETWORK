void CoheraSaturatorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), 2);

    // --- 1. ПАРАМЕТРЫ ---

    float driveParam = *apvts.getRawParameterValue("drive_master"); // 0..100
    float mixParam   = *apvts.getRawParameterValue("mix");          // 0..100
    float outDb      = *apvts.getRawParameterValue("output_gain");

    // Маппинг Драйва:
    // 0%   -> 1.0 (0 dB)
    // 100% -> 10.0 (+20 dB)
    // Это честный гейн.
    float targetDrive = 1.0f + (driveParam * 0.09f); 

    // Маппинг Компенсации (ПРЕДСКАЗУЕМЫЙ):
    // Чем больше Драйв, тем тише делаем выход Wet-сигнала.
    // Формула 1/sqrt(Drive) сохраняет энергию (Power Matching).
    // Если Drive=4 (+12dB), Comp=0.5 (-6dB). Итог: +6dB RMS (жир), но пики те же.
    float targetComp = 1.0f / std::sqrt(targetDrive);

    smoothedDrive.setTargetValue(targetDrive);
    smoothedCompensation.setTargetValue(targetComp); // Используем этот сглаживатель просто как параметр
    smoothedMix.setTargetValue(mixParam / 100.0f);
    smoothedOutput.setTargetValue(juce::Decibels::decibelsToGain(outDb));

    // --- 2. СЕТЬ (Envelope Follower) ---

    // Оставляем только базовую отправку, чтобы не ломать логику,
    // но пока отключим влияние на звук, чтобы настроить фундамент.
    currentGroup = (int)*apvts.getRawParameterValue("group_id");
    isReference  = (*apvts.getRawParameterValue("role") > 0.5f);

    if (isReference) {
        float maxPeak = buffer.getMagnitude(0, numSamples);
        float envValue = envelope.process(maxPeak);
        NetworkManager::getInstance().updateGroupSignal(currentGroup, envValue);
    }
    
    // Пока читаем, но не применяем, чтобы отладить чистоту звука!
    // float netVal = ...; 

    // --- 3. DRY COPY ---

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);

    juce::dsp::AudioBlock<float> dryBlock(dryBuffer);
    juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);
    dryDelayLine.process(dryContext);

    // --- 4. SPLIT ---

    filterBank->splitIntoBands(buffer, bandBufferPtrs.data(), numSamples);

    // --- 5. PROCESS LOOP ---
    
    // Настройка TILT: Саб (0) и Низ (1) сатурируем меньше, чтобы не засирать микс.
    const float bandDriveScale[6] = { 0.5f, 0.75f, 1.0f, 1.0f, 1.1f, 1.2f };

    auto* dryL = dryBuffer.getReadPointer(0);
    auto* dryR = (numCh > 1) ? dryBuffer.getReadPointer(1) : nullptr;
    auto* outL = buffer.getWritePointer(0);
    auto* outR = (numCh > 1) ? buffer.getWritePointer(1) : nullptr;

    for (int i = 0; i < numSamples; ++i)
    {
        float drv = smoothedDrive.getNextValue();
        float cmp = smoothedCompensation.getNextValue();
        float mix = smoothedMix.getNextValue();
        float outG = smoothedOutput.getNextValue();

        // Wet сумматор
        float wetL = 0.0f;
        float wetR = 0.0f;

        for (int band = 0; band < kNumBands; ++band)
        {
            // Персональный драйв для полосы
            float bDrive = drv * bandDriveScale[band];
            
            // Левый
            float xL = bandBuffers[band].getSample(0, i);
            // Классический Tanh: мягкое ограничение
            float satL = std::tanh(xL * bDrive);
            wetL += satL;

            // Правый
            if (outR) {
                float xR = bandBuffers[band].getSample(1, i);
                float satR = std::tanh(xR * bDrive);
                wetR += satR;
            }
        }

        // Применяем МАТЕМАТИЧЕСКУЮ компенсацию к Wet сумме
        // Это вернет уровень к вменяемым значениям
        wetL *= cmp;
        if (outR) wetR *= cmp;

        // MIX & OUTPUT
        
        // Линейный микс
        float finalL = dryL[i] * (1.0f - mix) + wetL * mix;
        finalL *= outG;

        // ЖЕЛЕЗНЫЙ ЛИМИТЕР (Brickwall)
        // Режем все, что выше -0.1 dB, чтобы не клиповало в DAW
        // Используем Hard Clip, потому что Tanh на мастере красит звук, а нам нужна прозрачность.
        finalL = std::max(-0.99f, std::min(0.99f, finalL));
        outL[i] = finalL;

        if (outR) {
            float finalR = dryR[i] * (1.0f - mix) + wetR * mix;
            finalR *= outG;
            finalR = std::max(-0.99f, std::min(0.99f, finalR));
            outR[i] = finalR;
        }
    }
    
    // Очистка
    for (int i = numCh; i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, numSamples);
}
