#include <iostream>
#include <cassert>
#include "src/dsp/InteractionEngine.h"
#include "src/parameters/ParameterSet.h"

using namespace Cohera;

int main() {
    std::cout << "=== Complete Network Ducking Test ===" << std::endl;
    
    // Настраиваем параметры для Unmasking (ducking)
    ParameterSet params;
    params.netMode = NetworkMode::Unmasking;
    params.netDepth = 1.0f;
    params.netSens = 1.0f;
    params.outputGain = 1.0f; // Базовый выходной gain
    
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
    float baseDriveMult = 1.0f;
    float baseOutputGain = params.outputGain;
    float depth = params.netDepth;
    
    // Drive модуляция
    float combinedDriveMult = baseDriveMult * (1.0f + targets.driveMod * depth);
    
    // Volume модуляция (громкость)
    // В BandProcessingEngine нет volume модуляции, но она может применяться в другом месте
    // Проверим как она должна работать:
    float combinedOutputGain = baseOutputGain * (1.0f + targets.volumeMod * depth);
    
    std::cout << "\n--- Drive Processing ---" << std::endl;
    std::cout << "Base drive: " << baseDriveMult << " → Combined: " << combinedDriveMult << std::endl;
    std::cout << "Drive change: " << ((combinedDriveMult < baseDriveMult) ? "REDUCED ✓" : "INCREASED ✗") << std::endl;
    
    std::cout << "\n--- Volume Processing ---" << std::endl;
    std::cout << "Base gain: " << baseOutputGain << " → Combined: " << combinedOutputGain << std::endl;
    std::cout << "Volume change: " << ((combinedOutputGain < baseOutputGain) ? "REDUCED ✓" : "INCREASED ✗") << std::endl;
    
    // Общий эффект (комбинированный результат)
    float totalEffect = combinedDriveMult * combinedOutputGain;
    std::cout << "\n--- Total Effect ---" << std::endl;
    std::cout << "Base total: " << (baseDriveMult * baseOutputGain) << " → Combined: " << totalEffect << std::endl;
    std::cout << "Total change: " << ((totalEffect < (baseDriveMult * baseOutputGain)) ? "REDUCED ✓" : "INCREASED ✗") << std::endl;
    
    // Проверяем, что общий эффект - это уменьшение громкости
    bool totalReduced = totalEffect < (baseDriveMult * baseOutputGain);
    bool driveReduced = combinedDriveMult < baseDriveMult;
    bool volumeReduced = combinedOutputGain < baseOutputGain;
    
    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Drive ducking: " << (driveReduced ? "✓" : "✗") << std::endl;
    std::cout << "Volume ducking: " << (volumeReduced ? "✓" : "✗") << std::endl;
    std::cout << "Total ducking: " << (totalReduced ? "✓" : "✗") << std::endl;
    
    bool allPass = driveReduced && volumeReduced && totalReduced;
    
    if (allPass) {
        std::cout << "\n🎉 COMPLETE DUCKING TEST PASSED!" << std::endl;
        std::cout << "Both drive and volume are correctly reduced." << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ COMPLETE DUCKING TEST FAILED!" << std::endl;
        std::cout << "Some modulation is not working correctly." << std::endl;
        return 1;
    }
}