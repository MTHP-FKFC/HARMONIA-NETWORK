#include <juce_core/juce_core.h>
#include "TestAudioGenerator.h"
#include "TestHelpers.h"
#include "../engine/ProcessingEngine.h"
#include "../network/NetworkManager.h"

// ==============================================================================
// SCENARIO 1: "The Fat Kick"
// Проверяем, что Sub-диапазон не разваливается при сильном драйве
// ==============================================================================

class ScenarioKickDrum : public juce::UnitTest
{
public:
    ScenarioKickDrum() : juce::UnitTest("Scenario: Fat Kick Stability") {}

    void runTest() override
    {
        Cohera::ProcessingEngine engine;
        double sr = 44100.0;
        engine.prepare({ sr, 1024, 2 });

        // 1. Генерируем Кик
        juce::AudioBuffer<float> buffer(2, 1024);
        juce::AudioBuffer<float> dryBuffer(2, 1024);
        CoheraTests::AudioGenerator::fillSyntheticKick(buffer, sr);
        dryBuffer.makeCopyOf(buffer);

        // 2. Настройки: Агрессивный драйв на низах
        Cohera::ParameterSet params;
        params.drive = 60.0f; // +12dB approx
        params.saturationMode = Cohera::SaturationMode::EulerTube; // Tube style
        params.preFilterFreq = 30.0f; // Cut ultra-lows
        params.mix = 1.0f;

        // 3. Process
        engine.processBlockWithDry(buffer, dryBuffer, params);

        // 4. Анализ
        // Уровень не должен улетать в +inf (защита DC Blocker и Limiters)
        float maxPeak = buffer.getMagnitude(0, 1024);
        expect(maxPeak < 2.0f, "Safety limiter should catch peaks");

        // Проверяем, что бас остался (энергия в сигнале есть)
        float rms = buffer.getRMSLevel(0, 0, 1024);
        expect(rms > 0.05f, "Output should not be silenced");

        // Проверяем работу DC Blocker (среднее значение должно быть около 0)
        float mean = 0.0f;
        auto* data = buffer.getReadPointer(0);
        for(int i=0; i<1024; ++i) mean += data[i];
        mean /= 1024.0f;

        expect(std::abs(mean) < 0.05f, "DC Offset should be removed");
    }
};

// ==============================================================================
// SCENARIO 2: "Network Unmasking" (Ducking)
// Имитируем взаимодействие двух плагинов: Бочка давит Бас
// ==============================================================================

class ScenarioNetworkDucking : public juce::UnitTest
{
public:
    ScenarioNetworkDucking() : juce::UnitTest("Scenario: Network Unmasking") {}

    void runTest() override
    {
        // 1. Инициализируем два движка
        Cohera::ProcessingEngine refEngine; // Kick (Sender)
        Cohera::ProcessingEngine listEngine; // Bass (Receiver)

        double sr = 44100.0;
        refEngine.prepare({ sr, 512, 2 });
        listEngine.prepare({ sr, 512, 2 });

        // 2. Сигналы
        juce::AudioBuffer<float> kickBuf(2, 512);
        juce::AudioBuffer<float> bassBuf(2, 512);

        CoheraTests::AudioGenerator::fillSyntheticKick(kickBuf, sr); // Loud Kick
        CoheraTests::fillSine(bassBuf, sr, 100.0f); // Constant Bass tone

        // Копии Dry
        juce::AudioBuffer<float> kickDry; kickDry.makeCopyOf(kickBuf);
        juce::AudioBuffer<float> bassDry; bassDry.makeCopyOf(bassBuf);

        // 3. Настраиваем параметры
        // Reference (Kick)
        Cohera::ParameterSet refParams;
        refParams.groupId = 1;
        refParams.netRole = Cohera::NetworkRole::Reference;

        // Listener (Bass)
        Cohera::ParameterSet listParams;
        listParams.groupId = 1;
        listParams.netRole = Cohera::NetworkRole::Listener;
        listParams.netMode = Cohera::NetworkMode::Unmasking; // DUCKING MODE
        listParams.netSens = 2.0f; // High sensitivity
        listParams.netDepth = 1.0f; // Full depth

        // 4. Process (Сначала Ref, чтобы отправить данные в сеть)
        refEngine.processBlockWithDry(kickBuf, kickDry, refParams);

        // Затем Listener (он должен прочитать данные и приглушиться)
        listEngine.processBlockWithDry(bassBuf, bassDry, listParams);

        // 5. Проверка
        // Бас должен стать тише!
        float dryRMS = bassDry.getRMSLevel(0, 0, 512);
        float wetRMS = bassBuf.getRMSLevel(0, 0, 512);

        // В режиме Unmasking с киком, бас должен быть придавлен
        expect(wetRMS < dryRMS * 0.9f, "Bass should be ducked by Kick signal via Network");

        // Очистка сети после теста (важно для других тестов)
        // (В идеале NetworkManager должен иметь reset, но пока просто не используем группу 1)
    }
};

// ==============================================================================
// SCENARIO 3: "Transient Punch"
// Проверяем, что Punch усиливает атаку
// ==============================================================================

class ScenarioTransientPunch : public juce::UnitTest
{
public:
    ScenarioTransientPunch() : juce::UnitTest("Scenario: Transient Punch") {}

    void runTest() override
    {
        Cohera::ProcessingEngine engine;
        double sr = 44100.0;
        engine.prepare({ sr, 512, 2 });

        juce::AudioBuffer<float> bufPunched(2, 512);
        juce::AudioBuffer<float> bufNeutral(2, 512);
        juce::AudioBuffer<float> dry(2, 512);

        // Резкий шум (Snare)
        CoheraTests::AudioGenerator::fillNoiseBurst(dry);

        bufPunched.makeCopyOf(dry);
        bufNeutral.makeCopyOf(dry);

        // 1. Process Neutral
        Cohera::ParameterSet p1;
        p1.punch = 0.0f;
        p1.drive = 20.0f;
        engine.processBlockWithDry(bufNeutral, dry, p1);

        // 2. Process with +100% Punch
        Cohera::ParameterSet p2;
        p2.punch = 1.0f; // Max Punch
        p2.drive = 20.0f;

        // Reset engine to clear envelopes
        engine.reset();
        engine.processBlockWithDry(bufPunched, dry, p2);

        // 3. Сравнение пиков
        // Punched версия должна иметь более высокий пик в начале (транзиент)
        // Берем первые 100 сэмплов
        float peakNeutral = bufNeutral.getMagnitude(0, 100);
        float peakPunched = bufPunched.getMagnitude(0, 100);

        expect(peakPunched > peakNeutral, "Positive Punch should increase transient peak level");
    }
};

// Регистрация
static ScenarioKickDrum testKick;
static ScenarioNetworkDucking testDuck;
static ScenarioTransientPunch testPunch;
