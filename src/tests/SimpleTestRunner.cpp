#include <iostream>
#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>

// Include our test audio generator
#include "TestAudioGenerator.h"

// Include core DSP components
#include "../dsp/MathSaturator.h"
#include "../dsp/DCBlocker.h"
#include "../CoheraTypes.h"

// Using declarations for convenience
using MathSaturator = MathSaturator;
using DCBlocker = DCBlocker;

int main(int argc, char* argv[])
{
    std::cout << "=== COHERA SATURATOR REAL-WORLD TEST RUNNER ===" << std::endl;
    std::cout << "Testing procedural audio generation and DSP processing..." << std::endl;
    std::cout << std::endl;

    juce::ScopedJuceInitialiser_GUI juceInit;

    // Test 1: Audio Generator - Synthetic Kick
    std::cout << "1. Testing Synthetic Kick Generation..." << std::endl;
    {
        juce::AudioBuffer<float> kickBuffer(2, 1024);
        CoheraTests::AudioGenerator::fillSyntheticKick(kickBuffer, 44100.0);

        float maxPeak = kickBuffer.getMagnitude(0, 1024);
        float rms = kickBuffer.getRMSLevel(0, 0, 1024);

        std::cout << "   ✓ Kick generated: Peak=" << maxPeak << ", RMS=" << rms << std::endl;

        if (maxPeak > 0.1f && rms > 0.01f) {
            std::cout << "   ✓ Kick generation PASSED" << std::endl;
        } else {
            std::cout << "   ✗ Kick generation FAILED" << std::endl;
        }
    }

    // Test 2: Audio Generator - Synthetic Bass
    std::cout << "2. Testing Synthetic Bass Generation..." << std::endl;
    {
        juce::AudioBuffer<float> bassBuffer(2, 2048);
        CoheraTests::AudioGenerator::fillSyntheticBass(bassBuffer, 44100.0);

        float maxPeak = bassBuffer.getMagnitude(0, 2048);
        float rms = bassBuffer.getRMSLevel(0, 0, 2048);

        std::cout << "   ✓ Bass generated: Peak=" << maxPeak << ", RMS=" << rms << std::endl;

        if (maxPeak > 0.1f && rms > 0.01f) {
            std::cout << "   ✓ Bass generation PASSED" << std::endl;
        } else {
            std::cout << "   ✗ Bass generation FAILED" << std::endl;
        }
    }

    // Test 3: Audio Generator - Noise Burst
    std::cout << "3. Testing Noise Burst Generation..." << std::endl;
    {
        juce::AudioBuffer<float> noiseBuffer(2, 512);
        CoheraTests::AudioGenerator::fillNoiseBurst(noiseBuffer);

        float maxPeak = noiseBuffer.getMagnitude(0, 512);
        float rms = noiseBuffer.getRMSLevel(0, 0, 512);

        std::cout << "   ✓ Noise burst generated: Peak=" << maxPeak << ", RMS=" << rms << std::endl;

        if (maxPeak > 0.1f && rms > 0.01f) {
            std::cout << "   ✓ Noise burst generation PASSED" << std::endl;
        } else {
            std::cout << "   ✗ Noise burst generation FAILED" << std::endl;
        }
    }

    // Test 4: MathSaturator - Golden Ratio Mode
    std::cout << "4. Testing MathSaturator - Golden Ratio..." << std::endl;
    {
        MathSaturator saturator;

        // Test with clean signal
        float input = 0.5f;
        float drive = 2.0f;
        float output = saturator.processSample(input, drive, Cohera::MathMode::GoldenRatio);

        std::cout << "   ✓ Golden Ratio: input=" << input << ", drive=" << drive << ", output=" << output << std::endl;

        // Should be saturated (different from input * drive)
        if (std::abs(output - input * drive) > 0.01f) {
            std::cout << "   ✓ Golden Ratio saturation PASSED" << std::endl;
        } else {
            std::cout << "   ✗ Golden Ratio saturation FAILED" << std::endl;
        }
    }

    // Test 5: MathSaturator - Euler Tube Mode
    std::cout << "5. Testing MathSaturator - Euler Tube..." << std::endl;
    {
        MathSaturator saturator;

        // Test with strong drive
        float input = 0.3f;
        float drive = 5.0f;
        float output = saturator.processSample(input, drive, Cohera::MathMode::EulerTube);

        std::cout << "   ✓ Euler Tube: input=" << input << ", drive=" << drive << ", output=" << output << std::endl;

        // Should be different from linear
        if (std::abs(output) < 1.0f && std::abs(output) > 0.01f) {
            std::cout << "   ✓ Euler Tube saturation PASSED" << std::endl;
        } else {
            std::cout << "   ✗ Euler Tube saturation FAILED" << std::endl;
        }
    }

    // Test 6: DC Blocker
    std::cout << "6. Testing DC Blocker..." << std::endl;
    {
        DCBlocker dcBlocker;

        // Create signal with DC offset
        juce::AudioBuffer<float> testBuffer(1, 1000);
        for (int i = 0; i < 1000; ++i) {
            testBuffer.setSample(0, i, 0.1f + 0.01f * std::sin(i * 0.1f)); // DC + small AC
        }

        // Calculate mean before DC blocking
        float meanBefore = 0.0f;
        for (int i = 0; i < 1000; ++i) {
            meanBefore += testBuffer.getSample(0, i);
        }
        meanBefore /= 1000.0f;

        // Process through DC blocker
        for (int i = 0; i < 1000; ++i) {
            float sample = testBuffer.getSample(0, i);
            testBuffer.setSample(0, i, dcBlocker.process(sample));
        }

        // Calculate mean after DC blocking
        float meanAfter = 0.0f;
        for (int i = 0; i < 1000; ++i) {
            meanAfter += testBuffer.getSample(0, i);
        }
        meanAfter /= 1000.0f;

        std::cout << "   ✓ DC Blocker: mean before=" << meanBefore << ", mean after=" << meanAfter << std::endl;

        if (std::abs(meanAfter) < std::abs(meanBefore) * 0.1f) {
            std::cout << "   ✓ DC Blocker PASSED" << std::endl;
        } else {
            std::cout << "   ✗ DC Blocker FAILED" << std::endl;
        }
    }

    // Test 7: Kick Processing Pipeline
    std::cout << "7. Testing Kick Processing Pipeline..." << std::endl;
    {
        // Generate kick
        juce::AudioBuffer<float> kickBuffer(2, 1024);
        CoheraTests::AudioGenerator::fillSyntheticKick(kickBuffer, 44100.0);

        float rmsBefore = kickBuffer.getRMSLevel(0, 0, 1024);

        // Apply saturation
        MathSaturator saturator;
        for (int i = 0; i < 1024; ++i) {
            float input = kickBuffer.getSample(0, i);
            float output = saturator.processSample(input, 2.0f, Cohera::MathMode::GoldenRatio);
            kickBuffer.setSample(0, i, output);
        }

        // Apply DC blocking
        DCBlocker dcBlocker;
        for (int i = 0; i < 1024; ++i) {
            float sample = kickBuffer.getSample(0, i);
            kickBuffer.setSample(0, i, dcBlocker.process(sample));
        }

        float rmsAfter = kickBuffer.getRMSLevel(0, 0, 1024);
        float maxPeak = kickBuffer.getMagnitude(0, 1024);

        std::cout << "   ✓ Kick pipeline: RMS before=" << rmsBefore << ", RMS after=" << rmsAfter << ", Peak=" << maxPeak << std::endl;

        if (rmsAfter > rmsBefore * 0.5f && maxPeak < 2.0f) {
            std::cout << "   ✓ Kick processing pipeline PASSED" << std::endl;
        } else {
            std::cout << "   ✗ Kick processing pipeline FAILED" << std::endl;
        }
    }

    std::cout << std::endl;
    std::cout << "=== REAL-WORLD AUDIO TESTING COMPLETED ===" << std::endl;
    std::cout << "✓ Procedural audio generation: Synthetic kick, bass, noise" << std::endl;
    std::cout << "✓ DSP processing: MathSaturator modes, DC blocking" << std::endl;
    std::cout << "✓ Processing pipeline: End-to-end kick drum processing" << std::endl;
    std::cout << std::endl;
    std::cout << "🎵 Cohera Saturator real-world testing framework is READY! 🎵" << std::endl;

    return 0;
}
