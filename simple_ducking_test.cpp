#include <iostream>
#include <cassert>
#include "src/dsp/InteractionEngine.h"
#include "src/parameters/ParameterSet.h"

using namespace Cohera;

int main() {
    std::cout << "=== Network Ducking Logic Test ===" << std::endl;
    
    // Настраиваем параметры для Unmasking (ducking)
    ParameterSet params;
    params.netMode = NetworkMode::Unmasking;
    params.netSens = 1.0f;
    
    // Тест 1: Громкий входной сигнал должен давать отрицательную модуляцию
    std::cout << "\n--- Test 1: Loud signal (0.8) ---" << std::endl;
    float loudSignal = 0.8f;
    auto targets1 = InteractionEngine::calculateModulation(
        NetworkMode::Unmasking, 
        loudSignal, 
        params.netSens
    );
    
    std::cout << "Input: " << loudSignal << std::endl;
    std::cout << "Drive modulation: " << targets1.driveMod << std::endl;
    std::cout << "Volume modulation: " << targets1.volumeMod << std::endl;
    
    bool loudTestPass = (targets1.driveMod < 0.0f) && (targets1.volumeMod < 0.0f);
    std::cout << "Result: " << (loudTestPass ? "✓ PASS" : "✗ FAIL") << std::endl;
    
    // Тест 2: Тихий входной сигнал должен давать минимальную модуляцию
    std::cout << "\n--- Test 2: Quiet signal (0.1) ---" << std::endl;
    float quietSignal = 0.1f;
    auto targets2 = InteractionEngine::calculateModulation(
        NetworkMode::Unmasking, 
        quietSignal, 
        params.netSens
    );
    
    std::cout << "Input: " << quietSignal << std::endl;
    std::cout << "Drive modulation: " << targets2.driveMod << std::endl;
    std::cout << "Volume modulation: " << targets2.volumeMod << std::endl;
    
    bool quietTestPass = (std::abs(targets2.driveMod) < 0.1f) && (std::abs(targets2.volumeMod) < 0.2f);
    std::cout << "Result: " << (quietTestPass ? "✓ PASS" : "✗ FAIL") << std::endl;
    
    // Тест 3: Нулевой сигнал должен давать нулевую модуляцию
    std::cout << "\n--- Test 3: Zero signal (0.0) ---" << std::endl;
    float zeroSignal = 0.0f;
    auto targets3 = InteractionEngine::calculateModulation(
        NetworkMode::Unmasking, 
        zeroSignal, 
        params.netSens
    );
    
    std::cout << "Input: " << zeroSignal << std::endl;
    std::cout << "Drive modulation: " << targets3.driveMod << std::endl;
    std::cout << "Volume modulation: " << targets3.volumeMod << std::endl;
    
    bool zeroTestPass = (targets3.driveMod == 0.0f) && (targets3.volumeMod == 0.0f);
    std::cout << "Result: " << (zeroTestPass ? "✓ PASS" : "✗ FAIL") << std::endl;
    
    // Общий результат
    std::cout << "\n=== Overall Results ===" << std::endl;
    bool allTestsPass = loudTestPass && quietTestPass && zeroTestPass;
    
    if (allTestsPass) {
        std::cout << "🎉 ALL TESTS PASSED! Network ducking logic is correct." << std::endl;
        return 0;
    } else {
        std::cout << "❌ SOME TESTS FAILED!" << std::endl;
        return 1;
    }
}