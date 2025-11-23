/**
 * Thermal Dynamics Analysis for HARMONIA NETWORK
 * 
 * This program generates test data to visualize the thermal behavior
 * of the saturation engine.
 */

#include <iostream>
#include <fstream>
#include <cmath>
#include <vector>

/**
 * Simulate thermal dynamics of saturation
 */
class ThermalSimulator {
private:
    float currentTemp = 20.0f; // Room temperature
    float targetTemp = 20.0f;
    const float heatingRate = 0.1f;
    const float coolingRate = 0.05f;
    const float maxTemp = 200.0f;
    
public:
    float process(float inputLevel, float drive) {
        // Calculate target temperature based on input and drive
        float energy = std::abs(inputLevel) * drive;
        targetTemp = 20.0f + energy * 180.0f; // Scale to 20-200°C
        
        // Thermal inertia - temperature follows target with lag
        float tempDiff = targetTemp - currentTemp;
        
        if (tempDiff > 0) {
            currentTemp += tempDiff * heatingRate; // Heating
        } else {
            currentTemp += tempDiff * coolingRate; // Cooling
        }
        
        currentTemp = std::max(20.0f, std::min(maxTemp, currentTemp));
        
        // Apply temperature-dependent saturation
        float tempFactor = (currentTemp - 20.0f) / (maxTemp - 20.0f);
        float saturation = 1.0f + tempFactor * 2.0f; // 1.0 to 3.0
        
        return std::tanh(inputLevel * saturation * drive);
    }
    
    float getTemperature() const { return currentTemp; }
};

/**
 * Generate thermal analysis data
 */
void runThermalAnalysis() {
    std::cout << "🔥 HARMONIA NETWORK - Thermal Dynamics Analysis" << std::endl;
    std::cout << "===============================================" << std::endl;
    
    ThermalSimulator simulator;
    
    std::ofstream csvFile("thermal_debug.csv");
    csvFile << "Time,Input,Output,Temperature,Drive\n";
    
    float sampleRate = 44100.0f;
    float duration = 10.0f; // 10 seconds
    int totalSamples = static_cast<int>(sampleRate * duration);
    
    // Test scenario: varying drive over time
    for (int i = 0; i < totalSamples; ++i) {
        float time = (float)i / sampleRate;
        
        // Generate test signal - complex waveform
        float fundamental = std::sin(2.0f * 3.14159f * 100.0f * time); // 100 Hz
        float harmonic2 = std::sin(2.0f * 3.14159f * 200.0f * time) * 0.3f; // 200 Hz
        float harmonic3 = std::sin(2.0f * 3.14159f * 300.0f * time) * 0.2f; // 300 Hz
        float input = (fundamental + harmonic2 + harmonic3) * 0.5f;
        
        // Dynamic drive that changes over time
        float drive = 0.3f; // Start with low drive
        
        if (time > 2.0f && time < 4.0f) {
            drive = 0.8f; // High drive period
        } else if (time > 4.0f && time < 6.0f) {
            drive = 0.1f; // Cool down period
        } else if (time > 6.0f && time < 8.0f) {
            drive = 0.6f; // Medium drive
        } else if (time >= 8.0f) {
            drive = 0.9f; // Maximum drive
        }
        
        // Process through thermal simulator
        float output = simulator.process(input, drive);
        float temperature = simulator.getTemperature();
        
        // Sample every 10th point to reduce file size
        if (i % 10 == 0) {
            csvFile << time << "," << input << "," << output << "," 
                    << temperature << "," << drive << "\n";
        }
        
        // Progress indicator
        if (i % (int)sampleRate == 0) {
            std::cout << "  Processing second " << (int)time + 1 << "/" << (int)duration 
                      << " (Temp: " << temperature << "°C)" << std::endl;
        }
    }
    
    csvFile.close();
    std::cout << std::endl;
    std::cout << "✅ Thermal data saved to thermal_debug.csv" << std::endl;
    std::cout << "📊 Run 'python visualize_thermal.py' to generate graphs" << std::endl;
}

int main() {
    try {
        runThermalAnalysis();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "❌ Error: " << e.what() << std::endl;
        return 1;
    }
}