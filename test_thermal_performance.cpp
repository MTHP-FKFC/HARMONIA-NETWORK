#include <iostream>
#include <iomanip>
#include <chrono>
#include <vector>

// Performance test for thermal model calculations
class ThermalPerformanceTest {
public:
    struct ThermalModel {
        float temperature = 20.0f;
        float thermalMass = 0.5f;
        float ambientTemp = 20.0f;
        
        inline void processSignal(float signalLevel) {
            // Optimized thermal calculation
            float power = signalLevel * signalLevel;
            float heating = power * 0.1f;
            float cooling = (temperature - ambientTemp) * 0.01f;
            temperature += heating - cooling;
            temperature = std::max(ambientTemp, std::min(150.0f, temperature));
        }
        
        void reset() {
            temperature = ambientTemp;
        }
    };
    
    void runPerformanceTest() {
        std::cout << "=== Thermal Performance Test ===" << std::endl;
        std::cout << "Testing thermal model performance..." << std::endl;
        
        const int NUM_SAMPLES = 44100 * 10; // 10 seconds at 44.1kHz
        const int NUM_CHANNELS = 2;
        const int NUM_ITERATIONS = 100; // Run multiple times for average
        
        std::vector<float> testSignals(NUM_SAMPLES);
        
        // Generate test signal (real-world audio simulation)
        for (int i = 0; i < NUM_SAMPLES; ++i) {
            // Mix of sine waves with noise (typical audio content)
            float signal = 0.3f * std::sin(2.0f * 3.14159f * 440.0f * i / 44100.0f);  // 440Hz
            signal += 0.2f * std::sin(2.0f * 3.14159f * 880.0f * i / 44100.0f);  // 880Hz
            signal += 0.1f * ((float)rand() / RAND_MAX - 0.5f);  // Noise
            testSignals[i] = signal;
        }
        
        std::vector<double> timings(NUM_ITERATIONS);
        
        for (int iter = 0; iter < NUM_ITERATIONS; ++iter) {
            ThermalModel thermal;
            thermal.reset();
            
            auto startTime = std::chrono::high_resolution_clock::now();
            
            // Process all samples (real-time simulation)
            for (int i = 0; i < NUM_SAMPLES; ++i) {
                thermal.processSignal(testSignals[i]);
            }
            
            auto endTime = std::chrono::high_resolution_clock::now();
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
            timings[iter] = duration.count();
        }
        
        // Calculate statistics
        double totalTime = 0.0;
        double minTime = timings[0];
        double maxTime = timings[0];
        
        for (double time : timings) {
            totalTime += time;
            minTime = std::min(minTime, time);
            maxTime = std::max(maxTime, time);
        }
        
        double avgTime = totalTime / NUM_ITERATIONS;
        
        // Performance metrics
        double samplesPerSecond = (double)NUM_SAMPLES / (avgTime / 1000000.0);
        double realTimeFactor = samplesPerSecond / 44100.0;
        double cpuPerSample = (avgTime / 1000000.0) / NUM_SAMPLES * 100.0;
        
        std::cout << "\nPerformance Results:" << std::endl;
        std::cout << "  Samples processed: " << NUM_SAMPLES << " (10 seconds at 44.1kHz)" << std::endl;
        std::cout << "  Average time: " << std::fixed << std::setprecision(2) << avgTime << " μs" << std::endl;
        std::cout << "  Min time: " << std::fixed << std::setprecision(2) << minTime << " μs" << std::endl;
        std::cout << "  Max time: " << std::fixed << std::setprecision(2) << maxTime << " μs" << std::endl;
        std::cout << "  Processing speed: " << std::fixed << std::setprecision(0) << samplesPerSecond << " samples/sec" << std::endl;
        std::cout << "  Real-time factor: " << std::fixed << std::setprecision(1) << realTimeFactor << "x" << std::endl;
        std::cout << "  CPU per sample: " << std::fixed << std::setprecision(6) << cpuPerSample << "%" << std::endl;
        
        // Performance assessment
        std::cout << "\nPerformance Assessment:" << std::endl;
        if (realTimeFactor > 100.0) {
            std::cout << "  ✓ EXCELLENT: Thermal processing is >100x real-time" << std::endl;
        } else if (realTimeFactor > 10.0) {
            std::cout << "  ✓ GOOD: Thermal processing is >10x real-time" << std::endl;
        } else if (realTimeFactor > 1.0) {
            std::cout << "  ⚠ ACCEPTABLE: Thermal processing is real-time capable" << std::endl;
        } else {
            std::cout << "  ❌ POOR: Thermal processing may impact real-time performance" << std::endl;
        }
        
        if (cpuPerSample < 0.001) {
            std::cout << "  ✓ EXCELLENT: CPU usage per sample is negligible" << std::endl;
        } else if (cpuPerSample < 0.01) {
            std::cout << "  ✓ GOOD: CPU usage per sample is minimal" << std::endl;
        } else {
            std::cout << "  ⚠ WARNING: CPU usage per sample may be significant" << std::endl;
        }
        
        std::cout << "\n=== Thermal Performance Test Complete ===" << std::endl;
    }
};

int main() {
    ThermalPerformanceTest test;
    test.runPerformanceTest();
    return 0;
}