#include <juce_core/juce_core.h>
#include "TestHelpers.h"
#include "TestAudioGenerator.h"
#include "../PluginProcessor.h"

class ComprehensiveParameterTests : public juce::UnitTest
{
public:
    ComprehensiveParameterTests() : juce::UnitTest("Safety: Dead Code Hunter (All Params)") {}

    // Хелпер: Проверяет, меняет ли параметр звук
    // Возвращает true, если звук изменился
    bool checkParamInfluence(juce::String paramID, float minVal, float maxVal, juce::String testName)
    {
        CoheraSaturatorAudioProcessor processor;
        double sr = 44100.0;
        int blockSize = 512;
        processor.prepareToPlay(sr, blockSize);

        // 1. Генерируем сложный сигнал (Шум + Бас), чтобы задеть все частоты
        juce::AudioBuffer<float> input(2, blockSize);
        CoheraTests::AudioGenerator::fillNoiseBurst(input); // Широкий спектр

        // Буферы для результатов
        juce::AudioBuffer<float> outMin(2, blockSize);
        juce::AudioBuffer<float> outMax(2, blockSize);
        juce::MidiBuffer midi;

        // 2. Рендер с MIN значением
        // Сбрасываем дефолты
        auto* pDrive = processor.getValueTreeState()->getRawParameterValue("drive_master");
        if (pDrive) *pDrive = 50.0f; // Ставим средний драйв, чтобы алгоритмы работали

        // Устанавливаем тестируемый параметр
        auto* param = processor.getValueTreeState()->getRawParameterValue(paramID);
        if (!param) {
            logMessage("CRITICAL ERROR: Parameter " + paramID + " not found in APVTS!");
            return false;
        }
        *param = minVal;

        // Прогрев (чтобы сглаживатели доехали)
        outMin.makeCopyOf(input);
        processor.processBlock(outMin, midi);
        outMin.makeCopyOf(input);
        processor.processBlock(outMin, midi); // Запись

        // 3. Рендер с MAX значением
        *param = maxVal;

        // Прогрев
        outMax.makeCopyOf(input);
        processor.processBlock(outMax, midi);
        outMax.makeCopyOf(input);
        processor.processBlock(outMax, midi); // Запись

        // 4. Сравнение
        bool hasChanged = !CoheraTests::areBuffersEqual(outMin, outMax, 0.00001f); // Высокая точность

        if (hasChanged) {
            // logMessage("OK: Parameter " + testName + " affects audio.");
        } else {
            logMessage("❌ FAIL: Parameter " + testName + " (" + paramID + ") has NO EFFECT on audio! Dead code?");
        }

        return hasChanged;
    }

    void runTest() override
    {
        // ====================================================================
        // GROUP 1: TONE SHAPING
        // ====================================================================
        beginTest("Tone Shaping Controls");

        expect(checkParamInfluence("tone_tighten", 10.0f, 500.0f, "Tighten (HPF)"),
               "Tighten should remove low end");

        expect(checkParamInfluence("tone_smooth", 22000.0f, 1000.0f, "Smooth (LPF)"),
               "Smooth should remove high end");

        expect(checkParamInfluence("dynamics", 0.0f, 100.0f, "Dynamics"),
               "Dynamics should change transient response");

        // ====================================================================
        // GROUP 2: PUNCH & TRANSIENTS
        // ====================================================================
        beginTest("Punch Engine");

        // Punch -100 vs 0 (Dirty vs Normal)
        expect(checkParamInfluence("punch", 0.0f, -100.0f, "Negative Punch"),
               "Negative Punch should dirty up the attack");

        // Punch 0 vs +100 (Normal vs Clean)
        expect(checkParamInfluence("punch", 0.0f, 100.0f, "Positive Punch"),
               "Positive Punch should enhance the attack");

        // ====================================================================
        // GROUP 3: ANALOG MOJO (Субъективные параметры)
        // ====================================================================
        beginTest("Analog Mojo Parameters");

        expect(checkParamInfluence("noise", 0.0f, 100.0f, "Noise Floor"),
               "Noise knob must add noise");

        expect(checkParamInfluence("analog_drift", 0.0f, 100.0f, "Drift"),
               "Drift should introduce bias offset");

        expect(checkParamInfluence("variance", 0.0f, 100.0f, "Stereo Variance"),
               "Variance should make L and R different");

        expect(checkParamInfluence("entropy", 0.0f, 100.0f, "Harmonic Entropy"),
               "Entropy should cause stochastic changes");

        // Heat сложнее, так как требует накопления энергии, но попробуем
        expect(checkParamInfluence("heat_amount", 0.0f, 100.0f, "Global Heat"),
               "Heat should affect drive/saturation characteristics");

        // ====================================================================
        // GROUP 4: QUALITY & MODES
        // ====================================================================
        beginTest("Modes and Quality");

        // Quality parameter exists and can be set (latency changes require host integration)

        expect(checkParamInfluence("math_mode", 0.0f, 2.0f, "Math Algo Switch"),
               "Switching algorithm (Golden -> PiFold) should change sound");

        // Note: sat_type parameter doesn't exist in this version

        // ====================================================================
        // GROUP 5: STEREO TOOLS
        // ====================================================================
        beginTest("Stereo Processing");

        expect(checkParamInfluence("focus", 0.0f, 100.0f, "Focus (Side Boost)"),
               "Focus should alter Mid/Side balance");

        expect(checkParamInfluence("delta", 0.0f, 1.0f, "Delta Monitoring"),
               "Delta should output difference signal");
    }
};

// Регистрация
static ComprehensiveParameterTests paramTest;
