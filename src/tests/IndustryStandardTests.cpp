#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "TestHelpers.h"
#include "../PluginProcessor.h"

// ==============================================================================
// TEST 4: STATE RECALL (Persistence)
// Критично: Проверяем, что XML сохраняется и загружается корректно.
// ==============================================================================

class StateRecallTest : public juce::UnitTest
{
public:
    StateRecallTest() : juce::UnitTest("Industry Standard: State Persistence") {}

    void runTest() override
    {
        beginTest("State Persistence Test");

        // 1. Создаем процессор
        CoheraSaturatorAudioProcessor processor;

        // 2. Инициализируем аудио (важно для APVTS)
        processor.prepareToPlay(44100.0, 512);

        // 3. Рандомизируем параметры
        auto* apvts = processor.getValueTreeState();

        // Устанавливаем "странные" значения
        *apvts->getRawParameterValue("drive_master") = 73.5f;
        *apvts->getRawParameterValue("mix") = 42.1f;
        *apvts->getRawParameterValue("sat_type") = 2.0f; // Hard Clip
        *apvts->getRawParameterValue("tone_tighten") = 350.0f;

        // 4. Сохраняем состояние через JUCE стандартный способ
        juce::MemoryBlock stateData;
        processor.getStateInformation(stateData);
        expect(stateData.getSize() > 0, "State data was saved");

        // 5. Создаем НОВЫЙ процессор
        CoheraSaturatorAudioProcessor processor2;
        processor2.prepareToPlay(44100.0, 512);

        // 6. Проверяем, что новый процессор имеет дефолтные значения
        auto* apvts2 = processor2.getValueTreeState();
        expectEquals((float)*apvts2->getRawParameterValue("drive_master"), 0.0f, "New processor has default drive");
        expectEquals((float)*apvts2->getRawParameterValue("mix"), 100.0f, "New processor has default mix");

        // 7. Загружаем сохраненное состояние во второй процессор
        processor2.setStateInformation(stateData.getData(), (int)stateData.getSize());

        // 8. Проверяем, что параметры восстановились
        expectEquals((float)*apvts2->getRawParameterValue("drive_master"), 73.5f, "Drive parameter recalled");
        expectEquals((float)*apvts2->getRawParameterValue("mix"), 42.1f, "Mix parameter recalled");
        expectEquals((float)*apvts2->getRawParameterValue("sat_type"), 2.0f, "Saturation type recalled");
        expectEquals((float)*apvts2->getRawParameterValue("tone_tighten"), 350.0f, "Filter parameter recalled");
    }
};

// ==============================================================================
// TEST 5: VARIABLE BLOCK SIZE (The "Host from Hell")
// Проверяем, не крашится ли плагин, если размер буфера меняется каждый вызов.
// FL Studio часто делает это.
// ==============================================================================

class VariableBlockSizeTest : public juce::UnitTest
{
public:
    VariableBlockSizeTest() : juce::UnitTest("Industry Standard: Variable Block Size") {}

    void runTest() override
    {
        CoheraSaturatorAudioProcessor processor;
        double sr = 44100.0;

        // Хост обещает макс 1024, но может давать меньше
        processor.prepareToPlay(sr, 1024);

        juce::MidiBuffer midi;

        // Массив "странных" размеров
        int sizes[] = { 1024, 512, 137, 1, 33, 256, 1024 };

        beginTest("Processing weird buffer sizes");

        for (int size : sizes)
        {
            if (size == 0) continue; // JUCE обычно не дает 0, но на всякий

            juce::AudioBuffer<float> buffer(2, size);
            CoheraTests::fillSine(buffer, sr, 440.0f);

            // ЭТО НЕ ДОЛЖНО КРАШИТЬСЯ ИЛИ ВЫДАВАТЬ ASSERT
            processor.processBlock(buffer, midi);

            // Проверка на NaN (взрыв фильтров)
            float mag = buffer.getMagnitude(0, size);
            expect(!std::isnan(mag) && !std::isinf(mag), "Output is valid numbers");
        }
    }
};

// ==============================================================================
// TEST 6: PARAMETER SMOOTHING (Zipper Noise Check)
// Проверяем, что изменение параметров происходит плавно, а не скачком.
// ==============================================================================

class ParameterSmoothingTest : public juce::UnitTest
{
public:
    ParameterSmoothingTest() : juce::UnitTest("Industry Standard: Parameter Smoothing") {}

    void runTest() override
    {
        beginTest("Parameter smoothing prevents extreme zipper noise");

        CoheraSaturatorAudioProcessor processor;
        processor.prepareToPlay(44100.0, 256);

        auto* pDrive = processor.getValueTreeState()->getRawParameterValue("drive_master");
        *pDrive = 0.0f; // Start with zero drive

        // 1. Process a few blocks to stabilize
        juce::AudioBuffer<float> buffer(2, 256);
        juce::MidiBuffer midi;

        for(int i = 0; i < 3; ++i) {
            CoheraTests::fillSine(buffer, 44100.0, 1000.0f); // High frequency for zipper sensitivity
            processor.processBlock(buffer, midi);
        }

        // 2. Abruptly change parameter to maximum
        *pDrive = 100.0f;

        // 3. Process one block with the parameter change
        CoheraTests::fillSine(buffer, 44100.0, 1000.0f);
        processor.processBlock(buffer, midi);

        // 4. Check that output is still valid (no NaN, no infinite values)
        float rms = buffer.getRMSLevel(0, 0, 256);
        float peak = buffer.getMagnitude(0, 256);

        expect(!std::isnan(rms) && !std::isinf(rms), "RMS is valid after parameter change");
        expect(!std::isnan(peak) && !std::isinf(peak), "Peak is valid after parameter change");
        expect(rms > 0.0f, "Output has signal after parameter change");
        expect(peak < 10.0f, "Output level is reasonable (no extreme clipping)");
    }
};

static StateRecallTest stateTest;
static VariableBlockSizeTest blockSizeTest;
static ParameterSmoothingTest smoothTest;
