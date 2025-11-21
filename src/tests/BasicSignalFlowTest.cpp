#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "TestHelpers.h" // Наши хелперы (fillSine, isSilent)
#include "../PluginProcessor.h"

class BasicSignalFlowTest : public juce::UnitTest
{
public:
    BasicSignalFlowTest() : juce::UnitTest("Fundamental: Signal Flow & Controls") {}

    void runTest() override
    {
        // 1. SETUP
        CoheraSaturatorAudioProcessor processor;
        double sr = 44100.0;
        int blockSize = 512;
        processor.prepareToPlay(sr, blockSize);

        // Ссылки на параметры для удобства
        auto* pDrive = processor.getValueTreeState()->getRawParameterValue("drive_master");
        auto* pMix = processor.getValueTreeState()->getRawParameterValue("mix");
        auto* pOut = processor.getValueTreeState()->getRawParameterValue("output_gain");
        auto* pMode = processor.getValueTreeState()->getRawParameterValue("math_mode");

        // Буферы
        juce::AudioBuffer<float> input(2, blockSize);
        juce::AudioBuffer<float> output(2, blockSize);
        juce::MidiBuffer midi;

        // Генерируем чистый синус 100Hz
        CoheraTests::fillSine(input, sr, 100.0f);

        // ====================================================================
        // TEST 1: SIGNAL PASS-THROUGH (Проходит ли звук?)
        // ====================================================================
        beginTest("Signal Chain Integrity");
        {
            // Дефолтные настройки
            *pDrive = 0.0f;
            *pMix = 100.0f;
            *pOut = 0.0f; // 0dB

            output.makeCopyOf(input);
            processor.processBlock(output, midi);

            // Проверяем, что выход не пустой
            expect(!CoheraTests::isSilent(output), "Signal should pass through the plugin");

            // Проверяем, что это не белый шум (RMS должен быть стабильным)
            float rms = output.getRMSLevel(0, 0, blockSize);
            expect(rms > 0.1f, "Output level should be healthy");
        }

        // ====================================================================
        // TEST 2: DRIVE CONTROL (Работает ли сатурация?)
        // ====================================================================
        beginTest("Drive Parameter Impact");
        {
            // A. LOW DRIVE
            *pDrive = 0.0f;
            *pMode = 0.0f; // Golden Ratio

            output.makeCopyOf(input);
            processor.processBlock(output, midi);
            float rmsLow = output.getRMSLevel(0, 0, blockSize);

            // Считаем пик-фактор (для синуса ~1.41)
            float peakLow = output.getMagnitude(0, blockSize);
            float crestLow = (rmsLow > 0) ? peakLow / rmsLow : 0.0f;

            // B. HIGH DRIVE
            *pDrive = 100.0f; // MAX POWER

            // "Прогреваем" фильтры (один блок в холостую)
            output.makeCopyOf(input);
            processor.processBlock(output, midi);

            output.makeCopyOf(input);
            processor.processBlock(output, midi);

            float rmsHigh = output.getRMSLevel(0, 0, blockSize);
            float peakHigh = output.getMagnitude(0, blockSize);
            float crestHigh = (rmsHigh > 0) ? peakHigh / rmsHigh : 0.0f;

            // ПРОВЕРКИ:

            // 1. Сатурация должна "сплющить" волну -> Crest Factor должен упасть
            // Синус (1.41) превращается в подобие квадрата (1.0)
            expect(crestHigh < crestLow, "High Drive should compress dynamic range (lower Crest Factor)");

            // 2. Сигнал должен стать субъективно громче (RMS растет)
            // (даже с нашей компенсацией, сатурация добавляет "жир")
            expect(rmsHigh > rmsLow, "High Drive should increase perceived loudness (RMS)");
        }

        // ====================================================================
        // TEST 3: OUTPUT GAIN (Работает ли ручка громкости?)
        // ====================================================================
        beginTest("Output Parameter Scaling");
        {
            *pDrive = 50.0f; // Средний драйв

            // A. 0 dB
            *pOut = 0.0f; // 0 dB (Gain 1.0)
            juce::AudioBuffer<float> bufA; bufA.makeCopyOf(input);
            processor.processBlock(bufA, midi);
            float rmsA = bufA.getRMSLevel(0, 0, blockSize);

            // B. -6 dB
            *pOut = -6.0f; // -6 dB (Gain ~0.5)
            juce::AudioBuffer<float> bufB; bufB.makeCopyOf(input);
            processor.processBlock(bufB, midi);
            float rmsB = bufB.getRMSLevel(0, 0, blockSize);

            // Проверка: RMS B должен быть примерно половиной от RMS A
            // Допускаем погрешность 10%
            float expectedRatio = juce::Decibels::decibelsToGain(-6.0f); // ~0.5
            float actualRatio = rmsB / rmsA;

            expectWithinAbsoluteError(actualRatio, expectedRatio, 0.1f, "Output knob should scale volume correctly (-6dB check)");
        }

        // ====================================================================
        // TEST 4: MIX CONTROL (Dry vs Wet)
        // ====================================================================
        beginTest("Mix Parameter Blending");
        {
            // Установим параметры так, чтобы Wet сильно отличался от Dry
            *pDrive = 100.0f;
            *pMode = 2.0f; // PiFold (сильно красит)
            *pOut = 0.0f;

            // A. DRY (0%)
            *pMix = 0.0f;
            juce::AudioBuffer<float> bufDry; bufDry.makeCopyOf(input);
            processor.processBlock(bufDry, midi);

            // B. WET (100%)
            *pMix = 100.0f;
            juce::AudioBuffer<float> bufWet; bufWet.makeCopyOf(input);
            processor.processBlock(bufWet, midi);

            // C. BLEND (50%)
            *pMix = 50.0f;
            juce::AudioBuffer<float> bufBlend; bufBlend.makeCopyOf(input);
            processor.processBlock(bufBlend, midi);

            // Проверки:

            // 1. Dry != Wet (они должны быть разными)
            expect(!CoheraTests::areBuffersEqual(bufDry, bufWet), "Dry and Wet signals must differ");

            // 2. Blend должен быть где-то посередине (по энергии)
            // Это грубая проверка, но достаточная
            float rmsDry = bufDry.getRMSLevel(0, 0, blockSize);
            float rmsWet = bufWet.getRMSLevel(0, 0, blockSize);
            float rmsBlend = bufBlend.getRMSLevel(0, 0, blockSize);

            // Если Wet громче Dry (из-за сатурации), то Blend должен быть между ними
            if (rmsWet > rmsDry) {
                expect(rmsBlend > rmsDry && rmsBlend < rmsWet, "Mix 50% energy should be between Dry and Wet");
            }
        }
    }
};

// Регистрация теста
static BasicSignalFlowTest basicTest;
