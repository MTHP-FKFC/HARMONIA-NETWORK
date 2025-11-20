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
