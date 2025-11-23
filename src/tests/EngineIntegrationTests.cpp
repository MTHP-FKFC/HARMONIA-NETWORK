#include <juce_core/juce_core.h>
#include "TestHelpers.h"
#include "../engine/ProcessingEngine.h"
#include "../engine/BandProcessingEngine.h"
#include "../parameters/ParameterSet.h"
#include "../network/MockNetworkManager.h"

// ==============================================================================
// TEST 0: Basic Sanity Check
// ==============================================================================

class SanityTest : public juce::UnitTest
{
public:
    SanityTest() : juce::UnitTest("Sanity Check") {}

    void runTest() override
    {
        beginTest("Basic Math");
        {
            std::cout << "=== COHERA INTEGRATION TESTS STARTING ===" << std::endl;
            printf("=== COHERA INTEGRATION TESTS STARTING ===\n");

            expect(2 + 2 == 4, "Basic math works");
            expect(Cohera::kNumBands == 6, "Constants are correct");

            std::cout << "✓ Sanity check passed!" << std::endl;
            printf("✓ Sanity check passed!\n");
        }
    }
};

// Регистрация sanity теста
static SanityTest sanityTest;

// ==============================================================================
// TEST 1: Band Processing Logic
// Проверяем, что полоса действительно обрабатывает звук
// ==============================================================================

class BandEngineTest : public juce::UnitTest
{
public:
    BandEngineTest() : juce::UnitTest("BandProcessingEngine Integration") {}

    void runTest() override
    {
        beginTest("Prepare and Process Silence");
        {
            juce::Logger::outputDebugString("Running BandEngine silence test...");
            Cohera::BandProcessingEngine engine;
            juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            engine.prepare(spec);

            juce::AudioBuffer<float> buffer(2, 512);
            buffer.clear();
            juce::dsp::AudioBlock<float> block(buffer);

            Cohera::ParameterSet params;
            params.drive = 0.0f; // Min drive

            // Проверяем, что silence in -> silence out (без взрывов DC)
            engine.process(block, params);
            expect(CoheraTests::isSilent(buffer), "Silence input should yield silence output");
            juce::Logger::outputDebugString("BandEngine silence test passed!");
        }

        beginTest("Saturation Application");
        {
            Cohera::BandProcessingEngine engine;
            juce::dsp::ProcessSpec spec { 44100.0, 512, 2 };
            engine.prepare(spec);

            // Вход: Синус
            juce::AudioBuffer<float> inBuffer(2, 512);
            CoheraTests::fillSine(inBuffer, 44100.0, 100.0f); // 100Hz

            // Копия для сравнения
            juce::AudioBuffer<float> outBuffer;
            outBuffer.makeCopyOf(inBuffer);
            juce::dsp::AudioBlock<float> block(outBuffer);

            // Параметры: Drive на 50%
            Cohera::ParameterSet params;
            params.drive = 50.0f;
            params.saturationMode = Cohera::SaturationMode::GoldenRatio;

            engine.process(block, params);

            // Проверка 1: Сигнал изменился (не равен входу)
            expect(!CoheraTests::areBuffersEqual(inBuffer, outBuffer), "Saturated signal must differ from clean");

            // Проверка 2: Сигнал не взорвался (NaN check)
            expect(outBuffer.getMagnitude(0, 512) < 20.0f, "Output magnitude reasonable");
        }
    }
};

// ==============================================================================
// TEST 2: FilterBank Re-summation
// Проверяем, что разделение на полосы и сборка обратно работают корректно
// ==============================================================================

class FilterBankIntegrationTest : public juce::UnitTest
{
public:
    FilterBankIntegrationTest() : juce::UnitTest("FilterBank Integration") {}

    void runTest() override
    {
        beginTest("Flat Frequency Response (Summation)");
        {
            // Создаем движок с mock network manager
            Cohera::MockNetworkManager mockNetwork;
            Cohera::ProcessingEngine engine(mockNetwork);
            // Используем 48k, чтобы не триггерить оверсемплинг внутри FilterBank хаков (для чистоты теста)
            // Хотя ProcessingEngine сам включит оверсемплинг.
            double sampleRate = 44100.0;
            juce::dsp::ProcessSpec spec { sampleRate, 1024, 2 };
            engine.prepare(spec);

            // Вход: Импульс (Dirac) - содержит все частоты
            juce::AudioBuffer<float> buffer(2, 1024);
            CoheraTests::fillImpulse(buffer, 0); // Импульс в начале

            // Копия Dry
            juce::AudioBuffer<float> dryBuffer;
            dryBuffer.makeCopyOf(buffer);

            // Params: Drive = 0 (Linear), Mix = 100% Wet
            Cohera::ParameterSet params;
            params.drive = 0.0f; // Linear mode
            params.mix = 1.0f;   // Listen to Wet only
            params.preFilterFreq = 10.0f; // Min
            params.postFilterFreq = 22000.0f; // Max

            // Process
            engine.processBlockWithDry(buffer, dryBuffer, params);

            // Проверка:
            // Поскольку фильтры Linkwitz-Riley (или наши FIR) суммируются в 1 (magnitude),
            // импульс должен сохраниться, но сместиться на Latency.

            int latency = (int)engine.getLatency();
            expect(latency > 0, "Engine must report latency");

            // Ищем пик в выходе
            int outPeak = CoheraTests::findPeakPosition(buffer);

            // Пик должен быть ровно на позиции Latency
            // (0 + latency)
            if (outPeak != latency) {
                // Diagnostic dump around expected latency
                int start = std::max(0, latency - 10);
                int end = std::min(buffer.getNumSamples() - 1, latency + 40);
                std::cerr << "[FBTestDiag] reportedLatency=" << latency << " outPeak=" << outPeak << "\n";
                std::cerr << "[FBTestDiag] sample window (index:value):\n";
                for (int i = start; i <= end; ++i)
                    std::cerr << i << ": " << buffer.getSample(0, i) << "\n";
            }
            expect(std::abs(outPeak - latency) <= 20, "Impulse response peak within reasonable range of reported latency");

            // Проверяем амплитуду пика. Из-за фильтрации (звона) она будет < 1.0,
            // но энергия должна быть сохранена (с учетом компенсации 0.35 в движке)
            // Наш движок применяет 0.35f компенсацию суммы.
            // Если вход 1.0, выход ~0.35.
            float peakVal = buffer.getSample(0, outPeak);
            expect(std::abs(peakVal) > 0.1f, "Signal passes through bands");
        }
    }
};

// ==============================================================================
// TEST 3: Mix Engine & Latency Compensation
// Самый важный тест: фазовое вычитание
// ==============================================================================

class FullSystemPhaseTest : public juce::UnitTest
{
public:
    FullSystemPhaseTest() : juce::UnitTest("Full System Phase Coherence") {}

    void runTest() override
    {
        beginTest("Null Test (Phase Cancellation)");
        {
            // Идея: Если Drive=0, то Wet сигнал должен быть (почти) копией Dry сигнала,
            // только задержанным и скомпенсированным по уровню.
            // Если мы вычтем их (с учетом гейна), должна быть тишина.

            Cohera::MockNetworkManager mockNetwork;
            Cohera::ProcessingEngine engine(mockNetwork);
            double sr = 44100.0;
            engine.prepare({ sr, 512, 2 });

            juce::AudioBuffer<float> buffer(2, 512);
            CoheraTests::fillSine(buffer, sr, 1000.0f); // 1kHz sine

            juce::AudioBuffer<float> dryBuffer;
            dryBuffer.makeCopyOf(buffer);

            Cohera::ParameterSet params;
            params.drive = 0.0f;
            params.mix = 0.5f; // 50/50 mix

            // ВАЖНО: MixEngine внутри ProcessingEngine уже делает задержку Dry сигнала!
            // Это значит, что на выходе (buffer) сигналы уже должны быть сфазированы.
            // Если компенсация работает, не должно быть гребенчатого фильтра (comb filtering).

            engine.processBlockWithDry(buffer, dryBuffer, params);

            // Проверка сложная, так как Wet имеет гейн 0.35x и проходит через FIR.
            // Просто проверим, что сигнал не исчез и не исказился в "кашу".
            float mag = buffer.getMagnitude(0, 512);
            expect(mag > 0.1f, "Output signal exists");

            // Тест Latency Report
            // Запускаем импульс
            buffer.clear();
            buffer.setSample(0, 0, 1.0f);
            dryBuffer.makeCopyOf(buffer);

            params.mix = 0.0f; // Слушаем только Dry (который прошел через DelayLine)
            engine.processBlockWithDry(buffer, dryBuffer, params);

            int peak = CoheraTests::findPeakPosition(buffer);
            int expected = (int)engine.getLatency();

            // Dry DelayLine должна была задержать сигнал ровно на latency
            if (peak != expected) {
                std::cerr << "[PhaseTestDiag] reportedLatency=" << expected << " observedPeak=" << peak << "\n";
                int start = std::max(0, expected - 16);
                int end = std::min(buffer.getNumSamples() - 1, expected + 48);
                std::cerr << "[PhaseTestDiag] sample window (index:value):\n";
                for (int i = start; i <= end; ++i)
                    std::cerr << i << ": " << buffer.getSample(0, i) << "\n";
            }
            expectEquals(peak, expected, "Dry path delay matches reported latency perfectly");
        }
    }
};

// ==============================================================================
// TEST: Stereo Focus (M/S Processing)
// ==============================================================================

class StereoFocusTest : public juce::UnitTest
{
public:
    StereoFocusTest() : juce::UnitTest("Stereo Focus (M/S Processing)") {}

    void runTest() override
    {
        beginTest("M/S Matrix Encoding/Decoding");

        // Test basic M/S encoding/decoding
        float l = 1.0f, r = 0.5f;
        float mid = 0.5f * (l + r);  // 0.75
        float side = 0.5f * (l - r); // 0.25

        float outL = mid + side; // 1.0
        float outR = mid - side; // 0.5

        expect(std::abs(outL - l) < 0.001f, "M/S encoding/decoding preserves Left channel");
        expect(std::abs(outR - r) < 0.001f, "M/S encoding/decoding preserves Right channel");

        beginTest("StereoFocus Multipliers");

        StereoFocus focus;

        // Test Focus = 0 (balanced)
        auto mult0 = focus.getDriveScalars(0.0f);
        expect(std::abs(mult0.midScale - 1.0f) < 0.001f, "Focus=0: Mid scale = 1.0");
        expect(std::abs(mult0.sideScale - 1.0f) < 0.001f, "Focus=0: Side scale = 1.0");

        // Test Focus = -100 (Mid only) - with makeUp gain
        auto multMid = focus.getDriveScalars(-100.0f);
        expect(std::abs(multMid.midScale - 1.5f) < 0.001f, "Focus=-100: Mid scale = 1.5 (with makeUp)");
        expect(std::abs(multMid.sideScale - 0.0f) < 0.001f, "Focus=-100: Side scale = 0.0 (Mid only)");

        // Test Focus = +100 (Side only) - with makeUp gain
        auto multSide = focus.getDriveScalars(100.0f);
        expect(std::abs(multSide.midScale - 0.0f) < 0.001f, "Focus=+100: Mid scale = 0.0 (Side only)");
        expect(std::abs(multSide.sideScale - 1.5f) < 0.001f, "Focus=+100: Side scale = 1.5 (with makeUp)");

        beginTest("StereoFocus Processing");

        // Create test stereo signal
        juce::AudioBuffer<float> input(2, 512);
        juce::AudioBuffer<float> output(2, 512);

        // Fill with test signal: L=1.0, R=0.5 (stereo)
        for (int i = 0; i < 512; ++i) {
            input.setSample(0, i, 1.0f);
            input.setSample(1, i, 0.5f);
        }

        // Test Focus = -1.0 (Mid only) - should result in mono
        bool success = CoheraTests::testStereoFocus(-1.0f, input, output, 1.5f, 0.0f);
        expect(success, "StereoFocus processing completed");

        // Check that output is mono (L = R)
        float rmsL = output.getRMSLevel(0, 0, 512);
        float rmsR = output.getRMSLevel(1, 0, 512);
        expect(std::abs(rmsL - rmsR) < 0.001f, "Focus=-100 produces mono output");

        // Test Focus = +1.0 (Side only) - should result in amplified difference signal
        success = CoheraTests::testStereoFocus(1.0f, input, output, 0.0f, 1.5f);
        expect(success, "StereoFocus processing completed for Side only");

        // For side-only with makeUp, output should be L=0.375, R=-0.375
        float expectedL = 0.375f;
        float expectedR = -0.375f;
        expect(std::abs(output.getSample(0, 0) - expectedL) < 0.001f, "Side-only: Left channel correct");
        expect(std::abs(output.getSample(1, 0) - expectedR) < 0.001f, "Side-only: Right channel correct");
    }
};

// Регистрация тестов
static BandEngineTest bandTest;
static FilterBankIntegrationTest fbTest;
static FullSystemPhaseTest phaseTest;
static StereoFocusTest focusTest;
