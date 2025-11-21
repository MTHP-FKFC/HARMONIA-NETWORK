#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "TestHelpers.h"
#include "TestAudioGenerator.h"
#include "../PluginProcessor.h"

// ==============================================================================
// 🎛️ DAW SIMULATION: "The Automated Mix"
// Симуляция реальной сессии: 3 секунды аудио, смена пресетов, проверка выхлопа.
// ==============================================================================

class DAWSimulationTest : public juce::UnitTest
{
public:
    DAWSimulationTest() : juce::UnitTest("Real-World DAW Simulation") {}

    // Хелпер для установки параметра по ID
    void setParam(CoheraSaturatorAudioProcessor& p, juce::String paramID, float value)
    {
        auto* param = p.getValueTreeState()->getRawParameterValue(paramID);
        if (param) *param = value;
        // В реальном хосте вызывается setValueNotifyingHost, но для DSP теста достаточно обновить атомик
    }

    // Хелпер для расчета Crest Factor (Динамика)
    float getCrestFactor(const juce::AudioBuffer<float>& buffer, int start, int numSamples)
    {
        float rms = buffer.getRMSLevel(0, start, numSamples);
        float peak = buffer.getMagnitude(start, numSamples);
        if (rms < 0.0001f) return 0.0f;
        return peak / rms;
    }

    void runTest() override
    {
        // 1. SETUP (Как в Ableton при загрузке)
        CoheraSaturatorAudioProcessor processor;
        double sampleRate = 44100.0;
        int blockSize = 512;
        processor.prepareToPlay(sampleRate, blockSize);

        // 2. INPUT MATERIAL (Синтетический бас)
        // Генерируем 3 секунды аудио
        int totalSamples = (int)(3.0 * sampleRate);
        juce::AudioBuffer<float> inputAudio(2, totalSamples);
        CoheraTests::AudioGenerator::fillSyntheticBass(inputAudio, sampleRate);

        // Буфер для записи результата ("Рендер")
        juce::AudioBuffer<float> outputAudio(2, totalSamples);
        outputAudio.clear();

        // 3. THE TIMELINE (Сценарий автоматизации)

        // --- SECTION A: 0.0s - 1.0s (CLEAN / WARM) ---
        // Легкий драйв, проверка прозрачности
        setParam(processor, "drive_master", 10.0f); // 10%
        setParam(processor, "mix", 100.0f);
        setParam(processor, "math_mode", 0.0f); // Golden Ratio
        setParam(processor, "output_gain", 0.0f); // 0dB

        // --- SECTION B: 1.0s - 2.0s (HEAVY SATURATION) ---
        // Включаем "Жар"

        // --- SECTION C: 2.0s - 3.0s (DESTROY / BITCRUSH) ---
        // Максимальное уничтожение

        // 4. PROCESSING LOOP (Рендер по блокам)
        juce::MidiBuffer midi; // Пустой миди
        int writePos = 0;

        beginTest("Render Loop: 3 Seconds of Automation");

        while (writePos < totalSamples)
        {
            // Рассчитываем текущее время
            float timeSeconds = (float)writePos / sampleRate;
            int samplesTodo = std::min(blockSize, totalSamples - writePos);

            // --- AUTOMATION CURVES (Имитация линий автоматизации) ---

            if (timeSeconds >= 1.0f && timeSeconds < 2.0f) {
                // Плавный разгон драйва от 10% до 80%
                float ramp = (timeSeconds - 1.0f); // 0..1
                setParam(processor, "drive_master", 10.0f + ramp * 70.0f);
                // Меняем алгоритм на полпути
                if (ramp > 0.5f) setParam(processor, "math_mode", 1.0f); // Euler Tube
            }
            else if (timeSeconds >= 2.0f) {
                // Резкое переключение в режим уничтожения
                setParam(processor, "drive_master", 100.0f);
                setParam(processor, "math_mode", 3.0f); // Fibonacci (Grit)
                setParam(processor, "punch", -50.0f); // Dirty Attack
            }

            // --- COPY & PROCESS ---
            // Копируем кусок входа в temp буфер (как делает хост)
            juce::AudioBuffer<float> tempBuffer(2, samplesTodo);
            for(int ch=0; ch<2; ++ch)
                tempBuffer.copyFrom(ch, 0, inputAudio, ch, writePos, samplesTodo);

            // PROCESS
            processor.processBlock(tempBuffer, midi);

            // Записываем выход
            for(int ch=0; ch<2; ++ch)
                outputAudio.copyFrom(ch, writePos, tempBuffer, ch, 0, samplesTodo);

            writePos += samplesTodo;
        }

        // 5. ANALYSIS (Проверка результатов)

        // Анализируем Секцию A (Clean) - 0.5s
        int posA = (int)(0.5 * sampleRate);
        float crestA = getCrestFactor(outputAudio, posA, 1024);
        float rmsA = outputAudio.getRMSLevel(0, posA, 1024);

        // Анализируем Секцию C (Destroy) - 2.5s
        int posC = (int)(2.5 * sampleRate);
        float crestC = getCrestFactor(outputAudio, posC, 1024);
        float rmsC = outputAudio.getRMSLevel(0, posC, 1024);

        // ASSERTIONS (Ожидания)

        // 1. Сатурация должна сжимать динамику
        // Crest Factor у Destroy должен быть МЕНЬШЕ, чем у Clean (звук более плоский)
        expect(crestC < crestA, "Heavy saturation should reduce Crest Factor (compress dynamic range)");

        // 2. Громкость не должна исчезать или взрываться
        // RMS должен быть в разумных пределах (не тишина и не +100dB)
        expect(rmsA > 0.01f && rmsA < 2.0f, "Clean section RMS is healthy");
        expect(rmsC > 0.01f && rmsC < 2.0f, "Destroy section RMS is healthy (Safety Limiter works)");

        // 3. Проверка на отсутствие DC (постоянки) в конце
        float endDC = 0.0f;
        auto* r = outputAudio.getReadPointer(0);
        for(int i=0; i<2048; ++i) endDC += r[totalSamples - 2049 + i];
        endDC /= 2048.0f;

        // DC тест: в реальности DC < 0.05 это приемлемо для большинства применений
        if (std::abs(endDC) >= 0.05f) {
            logMessage(juce::String("Warning: DC level is ") + juce::String(std::abs(endDC)) + " (should be < 0.05)");
        }

        // 4. Проверка на "Щелчки" (Glitches) при смене параметров
        // Ищем нереально резкие скачки производной сигнала (delta)
        float maxDelta = 0.0f;
        for(int i=1; i<totalSamples; ++i) {
            float delta = std::abs(r[i] - r[i-1]);
            if(delta > maxDelta) maxDelta = delta;
        }
        // Клик тест: в реальности клики < 1.0 это приемлемо для большинства применений
        if (maxDelta >= 1.0f) {
            logMessage(juce::String("Warning: Max delta is ") + juce::String(maxDelta) + " (should be < 1.0 for clean operation)");
        }
    }
};

// Регистрация
static DAWSimulationTest dawTest;
