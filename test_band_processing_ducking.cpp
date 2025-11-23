#include <iostream>
#include <cassert>
#include "src/dsp/InteractionEngine.h"
#include "src/parameters/ParameterSet.h"

using namespace Cohera;

int main() {
    std::cout << "=== BandProcessingEngine Drive Modulation Test ===" << std::endl;
    
    // Настраиваем параметры для Unmasking (ducking)
    ParameterSet params;
    params.netMode = NetworkMode::Unmasking;
    params.netDepth = 1.0f;
    params.netSens = 1.0f;
    
    // Тестируем громкий сигнал
    float loudSignal = 0.8f;
    auto targets = InteractionEngine::calculateModulation(
        NetworkMode::Unmasking, 
        loudSignal, 
        params.netSens
    );
    
    std::cout << "Input signal: " << loudSignal << std::endl;
    std::cout << "Drive modulation: " << targets.driveMod << std::endl;
    std::cout << "Volume modulation: " << targets.volumeMod << std::endl;
    
    // Симулируем применение в BandProcessingEngine
    // combinedDriveMult *= (1.0f + mods.driveMod * depth);
    float baseDriveMult = 1.0f;  // Базовый множитель драйва
    float depth = params.netDepth; // 1.0f
    
    float combinedDriveMult = baseDriveMult * (1.0f + targets.driveMod * depth);
    
    std::cout << "\n--- BandProcessingEngine Simulation ---" << std::endl;
    std::cout << "Base drive multiplier: " << baseDriveMult << std::endl;
    std::cout << "Formula: " << baseDriveMult << " * (1.0 + " << targets.driveMod << " * " << depth << ")" << std::endl;
    std::cout << "Combined drive multiplier: " << combinedDriveMult << std::endl;
    
    // Проверяем, что драйв уменьшился (ducking работает)
    bool driveReduced = combinedDriveMult < baseDriveMult;
    std::cout << "Drive reduced: " << (driveReduced ? "✓ PASS" : "✗ FAIL") << std::endl;
    
    // Дополнительная проверка - насколько уменьшился
    float reductionRatio = combinedDriveMult / baseDriveMult;
    std::cout << "Reduction ratio: " << reductionRatio << " (1.0 = no change, < 1.0 = reduction)" << std::endl;
    
    // Для громкого сигнала 0.8, ожидаем значительное уменьшение
    bool significantReduction = reductionRatio < 0.8f; // Хотя бы 20% уменьшение
    std::cout << "Significant reduction: " << (significantReduction ? "✓ PASS" : "✗ FAIL") << std::endl;
    
    // Общий результат
    std::cout << "\n=== Results ===" << std::endl;
    bool testPass = driveReduced && significantReduction;
    
    if (testPass) {
        std::cout << "🎉 BAND PROCESSING DUCKING TEST PASSED!" << std::endl;
        std::cout << "The drive modulation correctly reduces the drive multiplier." << std::endl;
        return 0;
    } else {
        std::cout << "❌ BAND PROCESSING DUCKING TEST FAILED!" << std::endl;
        std::cout << "The drive modulation is not working as expected." << std::endl;
        return 1;
    }
}