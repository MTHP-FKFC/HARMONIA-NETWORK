void CoheraSaturatorAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin(buffer.getNumChannels(), 2);

    // --- 1. ПАРАМЕТРЫ ---

    float driveParam = *apvts.getRawParameterValue("drive_master");
    float mixParam   = *apvts.getRawParameterValue("mix");
    float outDb      = *apvts.getRawParameterValue("output_gain");

    // Маппинг Drive: 0..100 -> 1.0..20.0
    float targetSatBlend = juce::jlimit(0.0f, 1.0f, driveParam / 10.0f);
    float driveRest = juce::jmax(0.0f, driveParam - 10.0f);
    float targetDrive = 1.0f + (driveRest * 0.2f); 

    smoothedDrive.setTargetValue(targetDrive);
    smoothedSatBlend.setTargetValue(targetSatBlend);
    smoothedMix.setTargetValue(mixParam / 100.0f);
    smoothedOutput.setTargetValue(juce::Decibels::decibelsToGain(outDb));

    // --- 2. АНАЛИЗ ВХОДА (RMS) ---

    // Считаем энергию входного блока
    float inputRMS = 0.0f;
    for (int ch = 0; ch < numCh; ++ch) {
        inputRMS += buffer.getRMSLevel(ch, 0, numSamples);
    }
    inputRMS /= (float)numCh;
    
    // Защита от деления на ноль (Noise Gate для детектора)
    // Если сигнал тише -60dB, считаем его тишиной и не меняем гейн
    if (inputRMS < 0.001f) inputRMS = 0.001f;

    // --- 3. СЕТЬ (Логика) ---

    currentGroup = (int)*apvts.getRawParameterValue("group_id");
    isReference  = (*apvts.getRawParameterValue("role") > 0.5f);
    int modeIndex = *apvts.getRawParameterValue("mode");

    if (isReference) {
        float envValue = envelope.process(inputRMS); // Шлем RMS, а не пики
        NetworkManager::getInstance().updateGroupSignal(currentGroup, envValue);
    }
    
    float rawNetVal = (!isReference) ? NetworkManager::getInstance().getGroupSignal(currentGroup) : 0.0f;
    smoothedNetworkSignal.setTargetValue(rawNetVal);

    // --- 4. СОЗДАЕМ DRY КОПИЮ (Для финала) ---

    juce::AudioBuffer<float> dryBuffer;
    dryBuffer.makeCopyOf(buffer);
    
    juce::dsp::AudioBlock<float> dryBlock(dryBuffer);
    juce::dsp::ProcessContextReplacing<float> dryContext(dryBlock);
    dryDelayLine.process(dryContext);

    // --- 5. SPLIT (Кроссовер) ---

    filterBank->splitIntoBands(buffer, bandBufferPtrs.data(), numSamples);

    // --- 6. SATURATION & SUM (WET) ---
    
    // Временный буфер для "Грязной Суммы" (чтобы измерить её громкость ДО микширования)
    juce::AudioBuffer<float> wetSumBuffer;
    wetSumBuffer.setSize(numCh, numSamples);
    wetSumBuffer.clear();

    // Получаем параметры для текущего блока
    float baseDrive = smoothedDrive.getNextValue(); smoothedDrive.skip(numSamples-1);
    float blendVal  = smoothedSatBlend.getNextValue(); smoothedSatBlend.skip(numSamples-1);
    float netVal    = smoothedNetworkSignal.getNextValue(); smoothedNetworkSignal.skip(numSamples-1);

    float controlSignal = scNormalizer.process(netVal);

    // Рассчитываем эффективный драйв (с учетом сети)
    float effectiveDrive = baseDrive;
    float ghostAmount = 0.0f;

    if (!isReference) {
        if (modeIndex == 0) effectiveDrive *= (1.0f - controlSignal * 0.8f); // Ducking
        else if (modeIndex == 1) { 
            ghostAmount = controlSignal; 
            effectiveDrive *= (1.0f + ghostAmount * 2.0f); // Boost for Ghost
        }
    }

    // Обработка полос
    for (int band = 0; band < kNumBands; ++band)
    {
        SaturationType type = SaturationType::WarmTube;
        float shaperMix = blendVal;

        if (ghostAmount > 0.01f && band <= 1) { 
            type = SaturationType::Rectifier;
            shaperMix = std::max(blendVal, ghostAmount); 
        }
        
        // Убрали bandDriveScale и старый comp. Чистая сатурация.
        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* src = bandBuffers[band].getReadPointer(ch);
            auto* dst = wetSumBuffer.getWritePointer(ch); // Складываем сразу в сумму
            
            for (int i = 0; i < numSamples; ++i)
            {
                float x = src[i] * effectiveDrive;
                float processed = shapers[band].processSample(x, 1.0f, type, shaperMix);
                dst[i] += processed; // Суммируем полосы
            }
        }
    }

    // --- 7. REAL AUTO-GAIN (Loudness Matcher) ---
    
    // Измеряем громкость того, что получилось (WET)
    float wetRMS = 0.0f;
    for (int ch = 0; ch < numCh; ++ch) {
        wetRMS += wetSumBuffer.getRMSLevel(ch, 0, numSamples);
    }
    wetRMS /= (float)numCh;
    if (wetRMS < 0.001f) wetRMS = 0.001f;

    // Вычисляем РЕАЛЬНЫЙ коэффициент разницы
    float requiredGain = inputRMS / wetRMS;

    // Ограничиваем безумие (не больше +18dB и не меньше -30dB)
    // Это защита от взрывов в тишине
    requiredGain = juce::jlimit(0.03f, 8.0f, requiredGain);

    // Отправляем в сглаживатель
    // autoGain (наш класс) используем теперь просто как сглаживатель
    // Или можем использовать smoothedOutput, но лучше отдельный SmoothedValue
    // Вспомним про smoothedCompensation из прошлого раза
    
    // ! ВАЖНО: Если мы в режиме Bypass (Drive ~ 0), форсируем gain = 1.0,
    // чтобы не было дрожания на чистом звуке.
    if (driveParam < 1.0f) requiredGain = 1.0f;

    smoothedCompensation.setTargetValue(requiredGain);

    // --- 8. FINAL MIX ---

    float currentMix = smoothedMix.getNextValue(); smoothedMix.skip(numSamples-1);
    float userOutGain = smoothedOutput.getNextValue(); smoothedOutput.skip(numSamples-1);

    // Копируем результат обратно в основной buffer
    for (int ch = 0; ch < numCh; ++ch)
    {
        auto* finalOut = buffer.getWritePointer(ch);
        const auto* dryData = dryBuffer.getReadPointer(ch);
        auto* wetData = wetSumBuffer.getWritePointer(ch);
        
        // Применяем сглаженную компенсацию к блоку Wet
        // (Для идеальной точности можно по-сэмплово, но applyGain быстрее и ок для RMS)
        // Используем smoothedCompensation.skip в конце
        float startGain = smoothedCompensation.getCurrentValue();
        float endGain = smoothedCompensation.getTargetValue(); // Грубое приближение для ramp
        
        // Применяем ramp gain к Wet буферу
        wetSumBuffer.applyGainRamp(ch, 0, numSamples, startGain, endGain);

        for (int i = 0; i < numSamples; ++i)
        {
            float dry = dryData[i];
            float wet = wetData[i]; // Уже скомпенсированный
            
            // Mix
            float mix = dry * (1.0f - currentMix) + wet * currentMix;
            
            // User Output Gain
            mix *= userOutGain;
            
            // Hard Clip / Safety Limiter на выходе (чтобы НИКОГДА не было > 0dB)
            // Если сигнал > 1.0, режем жестко.
            // Это единственный способ гарантировать отсутствие +15dB.
            mix = std::clamp(mix, -1.0f, 1.0f);
            
            finalOut[i] = mix;
        }
    }
    
    // Продвигаем сглаживатель состояния
    smoothedCompensation.skip(numSamples);
    
    // Очистка мусора
    for (auto i = numCh; i < getTotalNumOutputChannels(); ++i)
        buffer.clear(i, 0, numSamples);
}
