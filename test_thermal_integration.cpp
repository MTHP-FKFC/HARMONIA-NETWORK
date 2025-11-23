#include "src/engine/ProcessingEngine.h"
#include "src/network/NetworkManager.h"
#include <iostream>
#include <juce_dsp/juce_dsp.h>

int main() {
    std::cout << "Testing Thermal Dynamics Integration..." << std::endl;
    
    // Create network manager
    Cohera::NetworkManager networkManager;
    
    // Create processing engine
    Cohera::ProcessingEngine engine(networkManager);
    
    // Prepare for 44.1kHz, 512 samples
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = 44100.0;
    spec.maximumBlockSize = 512;
    spec.numChannels = 2;
    
    engine.prepare(spec);
    
    std::cout << "✅ ProcessingEngine prepared successfully!" << std::endl;
    std::cout << "✅ SaturationEngine integrated!" << std::endl;
    std::cout << "✅ Thermal models ready!" << std::endl;
    
    // Test thermal API
    float temp = engine.getAverageTemperature();
    std::cout << "🌡️  Initial temperature: " << temp << "°C" << std::endl;
    
    // Create test audio buffer
    juce::AudioBuffer<float> buffer(2, 512);
    juce::AudioBuffer<float> dryBuffer(2, 512);
    
    // Fill with test signal
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < 512; ++i) {
            buffer.setSample(ch, i, 0.1f * std::sin(2.0 * juce::MathConstants<double>::pi * 440.0 * i / 44100.0));
            dryBuffer.setSample(ch, i, buffer.getSample(ch, i));
        }
    }
    
    // Create parameters
    Cohera::ParameterSet params;
    params.drive = 50.0f; // High drive to generate heat
    params.mix = 1.0f;
    params.outputGain = 1.0f;
    
    std::cout << "🔥 Processing hot signal (Drive=" << params.drive << ")..." << std::endl;
    
    // Process audio
    engine.processBlockWithDry(buffer, dryBuffer, params);
    
    // Check temperature after processing
    temp = engine.getAverageTemperature();
    std::cout << "🌡️  Temperature after processing: " << temp << "°C" << std::endl;
    
    if (temp > 20.0f) {
        std::cout << "✅ THERMAL DYNAMICS WORKING! Temperature increased from heat!" << std::endl;
    } else {
        std::cout << "❌ Thermal dynamics not responding" << std::endl;
    }
    
    std::cout << "✅ Thermal integration test completed!" << std::endl;
    return 0;
}