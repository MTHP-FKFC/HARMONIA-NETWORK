#include "src/PluginProcessor.h"
#include "src/engine/ProcessingEngine.h"
#include "src/parameters/ParameterSet.h"
#include <juce_dsp/juce_dsp.h>
#include <iostream>
#include <iomanip>

class ThermalResponseTest {
public:
    void runTest() {
        std::cout << "=== Thermal Response Test ===" << std::endl;
        
        // Create plugin instance
        CoheraSaturatorAudioProcessor processor;
        
        // Prepare for processing
        const double sampleRate = 44100.0;
        const int blockSize = 512;
        processor.prepareToPlay(sampleRate, blockSize);
        
        // Create test signal - sine wave at 1kHz
        juce::AudioBuffer<float> testBuffer(2, blockSize);
        juce::AudioBuffer<float> dryBuffer(2, blockSize);
        
        // Test 1: Low drive (should produce minimal heat)
        std::cout << "\nTest 1: Low Drive (10%)" << std::endl;
        testLowDrive(processor, testBuffer, dryBuffer);
        
        // Test 2: High drive (should produce significant heat)
        std::cout << "\nTest 2: High Drive (80%)" << std::endl;
        testHighDrive(processor, testBuffer, dryBuffer);
        
        // Test 3: Thermal decay over time
        std::cout << "\nTest 3: Thermal Decay (no signal)" << std::endl;
        testThermalDecay(processor, testBuffer, dryBuffer);
        
        std::cout << "\n=== Thermal Test Complete ===" << std::endl;
    }
    
private:
    void generateTestSignal(juce::AudioBuffer<float>& buffer, float frequency = 1000.0f, float amplitude = 0.5f) {
        const double sampleRate = 44100.0;
        const int numSamples = buffer.getNumSamples();
        const int numChannels = buffer.getNumChannels();
        
        for (int sample = 0; sample < numSamples; ++sample) {
            float value = amplitude * std::sin(2.0 * juce::MathConstants<double>::pi * frequency * sample / sampleRate);
            for (int channel = 0; channel < numChannels; ++channel) {
                buffer.setSample(channel, sample, value);
            }
        }
    }
    
    void testLowDrive(CoheraSaturatorAudioProcessor& processor, 
                     juce::AudioBuffer<float>& testBuffer,
                     juce::AudioBuffer<float>& dryBuffer) {
        // Set low drive
        auto& apvts = processor.getParameters();
        auto* driveParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("drive_master"));
        if (driveParam) {
            driveParam->setValueNotifyingHost(0.1f); // 10% drive
        }
        
        // Generate moderate test signal
        generateTestSignal(testBuffer, 1000.0f, 0.3f);
        dryBuffer.makeCopyOf(testBuffer);
        
        // Process multiple blocks to build up heat
        float startTemp = processor.getAverageTemperature();
        for (int i = 0; i < 10; ++i) {
            processor.processBlock(testBuffer, dryBuffer);
        }
        float endTemp = processor.getAverageTemperature();
        
        std::cout << "  Start temperature: " << std::fixed << std::setprecision(2) << startTemp << "°C" << std::endl;
        std::cout << "  End temperature: " << std::fixed << std::setprecision(2) << endTemp << "°C" << std::endl;
        std::cout << "  Temperature increase: " << std::fixed << std::setprecision(2) << (endTemp - startTemp) << "°C" << std::endl;
        
        if (endTemp > startTemp + 0.5f) {
            std::cout << "  ✓ Thermal response detected" << std::endl;
        } else {
            std::cout << "  ⚠ Minimal thermal response (expected for low drive)" << std::endl;
        }
    }
    
    void testHighDrive(CoheraSaturatorAudioProcessor& processor,
                      juce::AudioBuffer<float>& testBuffer,
                      juce::AudioBuffer<float>& dryBuffer) {
        // Set high drive
        auto& apvts = processor.getParameters();
        auto* driveParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("drive_master"));
        if (driveParam) {
            driveParam->setValueNotifyingHost(0.8f); // 80% drive
        }
        
        // Generate strong test signal
        generateTestSignal(testBuffer, 1000.0f, 0.8f);
        dryBuffer.makeCopyOf(testBuffer);
        
        // Process multiple blocks to build up heat
        float startTemp = processor.getAverageTemperature();
        for (int i = 0; i < 20; ++i) {
            processor.processBlock(testBuffer, dryBuffer);
        }
        float endTemp = processor.getAverageTemperature();
        
        std::cout << "  Start temperature: " << std::fixed << std::setprecision(2) << startTemp << "°C" << std::endl;
        std::cout << "  End temperature: " << std::fixed << std::setprecision(2) << endTemp << "°C" << std::endl;
        std::cout << "  Temperature increase: " << std::fixed << std::setprecision(2) << (endTemp - startTemp) << "°C" << std::endl;
        
        if (endTemp > startTemp + 2.0f) {
            std::cout << "  ✓ Strong thermal response detected" << std::endl;
        } else {
            std::cout << "  ⚠ Unexpectedly low thermal response" << std::endl;
        }
    }
    
    void testThermalDecay(CoheraSaturatorAudioProcessor& processor,
                         juce::AudioBuffer<float>& testBuffer,
                         juce::AudioBuffer<float>& dryBuffer) {
        // Set drive to 0
        auto& apvts = processor.getParameters();
        auto* driveParam = dynamic_cast<juce::AudioParameterFloat*>(apvts.getParameter("drive_master"));
        if (driveParam) {
            driveParam->setValueNotifyingHost(0.0f); // 0% drive
        }
        
        // Clear buffers (silence)
        testBuffer.clear();
        dryBuffer.clear();
        
        // Measure thermal decay
        float startTemp = processor.getAverageTemperature();
        for (int i = 0; i < 50; ++i) {
            processor.processBlock(testBuffer, dryBuffer);
            if (i % 10 == 0) {
                float currentTemp = processor.getAverageTemperature();
                std::cout << "  Block " << i << ": " << std::fixed << std::setprecision(2) << currentTemp << "°C" << std::endl;
            }
        }
        float endTemp = processor.getAverageTemperature();
        
        std::cout << "  Temperature decay: " << std::fixed << std::setprecision(2) << (startTemp - endTemp) << "°C" << std::endl;
        
        if (endTemp < startTemp - 0.5f) {
            std::cout << "  ✓ Thermal decay working" << std::endl;
        } else {
            std::cout << "  ⚠ Minimal thermal decay" << std::endl;
        }
    }
};

int main() {
    ThermalResponseTest test;
    test.runTest();
    return 0;
}