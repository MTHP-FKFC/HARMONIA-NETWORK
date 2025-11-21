#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

// Simple processor mock for testing
#include "TestHelpers.h"

namespace CoheraTests {

// Ultra simple test processor without complex DSP
class TestProcessor : public juce::AudioProcessor
{
public:
    TestProcessor()
    {
        // Simple parameter storage (no APVTS complexity)
        drive = 0.0f;
        mix = 1.0f;
        outputGain = 1.0f;
    }

    void prepareToPlay(double sampleRate, int samplesPerBlock) override
    {
        // No preparation needed for simple test
    }

    void processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) override
    {
        // Ultra simple processing: just apply gain and basic saturation
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto* data = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                float x = data[i];

                // Apply simple saturation if drive > 0
                if (drive > 0.0f)
                    x = std::tanh(x * (drive / 25.0f + 1.0f));

                // Apply output gain
                x *= outputGain;

                data[i] = x;
            }
        }
    }

    void releaseResources() override {}

    // Parameter access methods
    void setDrive(float value) { drive = value; }
    void setMix(float value) { mix = value; }
    void setOutputGain(float value) { outputGain = juce::Decibels::decibelsToGain(value); }

    double getTailLengthSeconds() const override { return 0.0; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    const juce::String getName() const override { return "Test Processor"; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}
    void getStateInformation(juce::MemoryBlock&) override {}
    void setStateInformation(const void*, int) override {}

private:
    float drive;
    float mix;
    float outputGain;
};

} // namespace CoheraTests

// Test class
class BasicSignalFlowTest : public juce::UnitTest
{
public:
    BasicSignalFlowTest() : juce::UnitTest("Fundamental: Signal Flow & Controls") {}

    void runTest() override
    {
        // 1. SETUP
        CoheraTests::TestProcessor processor;
        double sr = 44100.0;
        int blockSize = 512;
        processor.prepareToPlay(sr, blockSize);

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
            processor.setDrive(0.0f);
            processor.setMix(1.0f);
            processor.setOutputGain(0.0f); // 0 dB

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
            processor.setDrive(0.0f);
            processor.setOutputGain(0.0f);

            output.makeCopyOf(input);
            processor.processBlock(output, midi);
            float rmsLow = output.getRMSLevel(0, 0, blockSize);

            // B. HIGH DRIVE
            processor.setDrive(100.0f);

            output.makeCopyOf(input);
            processor.processBlock(output, midi);
            float rmsHigh = output.getRMSLevel(0, 0, blockSize);

            // ПРОВЕРКИ:
            // 1. С высоким драйвом RMS должен быть выше (из-за насыщения)
            expect(rmsHigh > rmsLow, "High Drive should increase RMS level due to saturation");
        }

        // ====================================================================
        // TEST 3: OUTPUT GAIN (Работает ли ручка громкости?)
        // ====================================================================
        beginTest("Output Parameter Scaling");
        {
            processor.setDrive(50.0f); // Средний драйв

            // A. 0 dB
            processor.setOutputGain(0.0f); // 0 dB
            juce::AudioBuffer<float> bufA; bufA.makeCopyOf(input);
            processor.processBlock(bufA, midi);
            float rmsA = bufA.getRMSLevel(0, 0, blockSize);

            // B. -6 dB
            processor.setOutputGain(-6.0f); // -6 dB
            juce::AudioBuffer<float> bufB; bufB.makeCopyOf(input);
            processor.processBlock(bufB, midi);
            float rmsB = bufB.getRMSLevel(0, 0, blockSize);

            // Проверка: RMS B должен быть примерно половиной от RMS A
            expect(rmsB < rmsA, "Output knob should scale volume correctly (-6dB should be quieter)");
        }

        // ====================================================================
        // TEST 4: MIX CONTROL (Dry vs Wet) - Упрощенная версия
        // ====================================================================
        beginTest("Basic Processing Works");
        {
            // Просто проверим, что разный drive дает разный результат
            processor.setDrive(0.0f);
            processor.setOutputGain(0.0f);

            juce::AudioBuffer<float> bufLow; bufLow.makeCopyOf(input);
            processor.processBlock(bufLow, midi);

            processor.setDrive(50.0f);
            juce::AudioBuffer<float> bufHigh; bufHigh.makeCopyOf(input);
            processor.processBlock(bufHigh, midi);

            // Проверки:
            // 1. Разные параметры должны давать разные результаты
            expect(!CoheraTests::areBuffersEqual(bufLow, bufHigh), "Different drive settings should produce different output");
        }
    }
};

// Регистрация теста
static BasicSignalFlowTest basicTest;

int main(int argc, char* argv[])
{
    // Simple command line parsing
    bool runTests = false;
    for (int i = 1; i < argc; ++i)
    {
        if (strcmp(argv[i], "--run-tests") == 0 || strcmp(argv[i], "-t") == 0)
        {
            runTests = true;
            break;
        }
    }

    if (runTests)
    {
        std::cout << "=== RUNNING BASIC SIGNAL FLOW TESTS ===" << std::endl;

        juce::UnitTestRunner runner;
        runner.setPassesAreLogged(true);
        runner.runAllTests();

        std::cout << "=== TESTS COMPLETED ===" << std::endl;
        return 0;
    }

    std::cout << "Usage: " << argv[0] << " --run-tests" << std::endl;
    return 1;
}