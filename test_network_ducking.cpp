#include <iostream>
#include <cassert>
#include "../src/network/NetworkController.h"
#include "../src/network/MockNetworkManager.h"
#include "../src/parameters/ParameterSet.h"
#include "../src/dsp/InteractionEngine.h"

using namespace Cohera;

int main() {
    std::cout << "=== Network Ducking Test ===" << std::endl;
    
    // Создаем mock network manager
    MockNetworkManager networkManager;
    
    // Создаем NetworkController
    NetworkController controller(networkManager);
    controller.prepare(44100.0);
    
    // Настраиваем параметры для Unmasking (ducking)
    ParameterSet params;
    params.netMode = NetworkMode::Unmasking;
    params.netRole = NetworkRole::Listener;
    params.groupId = 0;
    params.netDepth = 1.0f;
    params.netSens = 1.0f;
    
    // Симулируем Reference отправляющий громкий сигнал (0.8)
    networkManager.updateBandSignal(0, 0, 0.8f);
    
    // Создаем пустой буфер (для Listener не важен вход)
    juce::AudioBuffer<float> buffer(1, 512);
    buffer.clear();
    
    // Получаем модуляцию
    auto modulations = controller.process(buffer, params);
    
    std::cout << "Input signal: 0.8" << std::endl;
    std::cout << "Network modulation (band 0): " << modulations[0] << std::endl;
    
    // Проверяем InteractionEngine
    auto targets = InteractionEngine::calculateModulation(
        NetworkMode::Unmasking, 
        modulations[0], 
        params.netSens
    );
    
    std::cout << "Drive modulation: " << targets.driveMod << std::endl;
    std::cout << "Volume modulation: " << targets.volumeMod << std::endl;
    
    // Для ducking должны быть отрицательные значения
    bool driveDucking = targets.driveMod < 0.0f;
    bool volumeDucking = targets.volumeMod < 0.0f;
    
    std::cout << "\n=== Results ===" << std::endl;
    std::cout << "Drive ducking: " << (driveDucking ? "✓ PASS" : "✗ FAIL") << std::endl;
    std::cout << "Volume ducking: " << (volumeDucking ? "✓ PASS" : "✗ FAIL") << std::endl;
    
    if (driveDucking && volumeDucking) {
        std::cout << "\n🎉 Network ducking test PASSED!" << std::endl;
        return 0;
    } else {
        std::cout << "\n❌ Network ducking test FAILED!" << std::endl;
        return 1;
    }
}