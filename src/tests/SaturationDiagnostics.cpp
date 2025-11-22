#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>
#include "TestHelpers.h"
#include "../PluginProcessor.h"
#include "../parameters/ParameterSet.h"
#include "../dsp/MathSaturator.h"

// Диагностический тест для проверки сатурации
class SaturationDiagnostics : public juce::UnitTest
{
public:
    SaturationDiagnostics() : juce::UnitTest("Saturation Diagnostics") {}

    void runTest() override
    {
        beginTest("Drive Saturation Analysis");
        
        CoheraSaturatorAudioProcessor processor;
        double sr = 44100.0;
        int blockSize = 512;
        processor.prepareToPlay(sr, blockSize);
        
        auto* apvts = processor.getValueTreeState();
        auto* pDrive = apvts->getRawParameterValue("drive_master");
        auto* pMode = apvts->getRawParameterValue("math_mode");
        
        juce::AudioBuffer<float> input(2, blockSize);
        juce::AudioBuffer<float> output(2, blockSize);
        juce::MidiBuffer midi;
        
        // Генерируем чистый синус 440Hz
        CoheraTests::fillSine(input, sr, 440.0f);
        
        // Тестируем разные режимы драйва
        float driveValues[] = {0.0f, 25.0f, 50.0f, 75.0f, 100.0f};
        int numModes = 3; // Тестируем первые 3 режима
        
        logMessage("=== SATURATION DIAGNOSTICS ===");
        logMessage("Input: 440Hz sine wave, blockSize=" + juce::String(blockSize));
        logMessage("");
        
        for (int mode = 0; mode < numModes; ++mode)
        {
            *pMode = (float)mode;
            logMessage("--- Mode " + juce::String(mode) + " ---");
            
            for (float drive : driveValues)
            {
                *pDrive = drive;
                
                // Вычисляем эффективный drive gain
                Cohera::ParameterSet params;
                params.drive = drive;
                float effectiveDrive = params.getEffectiveDriveGain();
                
                output.makeCopyOf(input);
                processor.processBlock(output, midi);
                
                // Анализ
                float rms = output.getRMSLevel(0, 0, blockSize);
                float peak = output.getMagnitude(0, blockSize);
                float crest = (rms > 0) ? peak / rms : 0.0f;
                
                // Проверяем, сколько сэмплов клипнули
                int clipped = 0;
                for (int i = 0; i < blockSize; ++i)
                {
                    float sample = output.getSample(0, i);
                    if (std::abs(sample) > 0.95f) clipped++;
                }
                
                logMessage(juce::String::formatted(
                    "Drive=%.1f%% (gain=%.2fx): RMS=%.4f, Peak=%.4f, Crest=%.3f, Clipped=%d/512",
                    drive, effectiveDrive, rms, peak, crest, clipped
                ));
            }
            logMessage("");
        }
        
        // Тест: Проверяем, что при drive=100% сатурация действительно агрессивна
        logMessage("=== AGGRESSIVENESS TEST ===");
        *pDrive = 0.0f;
        *pMode = 0.0f; // Golden Ratio
        
        output.makeCopyOf(input);
        processor.processBlock(output, midi);
        float rmsLow = output.getRMSLevel(0, 0, blockSize);
        float peakLow = output.getMagnitude(0, blockSize);
        float crestLow = (rmsLow > 0) ? peakLow / rmsLow : 0.0f;
        
        *pDrive = 100.0f;
        output.makeCopyOf(input);
        processor.processBlock(output, midi);
        output.makeCopyOf(input);
        processor.processBlock(output, midi);
        
        float rmsHigh = output.getRMSLevel(0, 0, blockSize);
        float peakHigh = output.getMagnitude(0, blockSize);
        float crestHigh = (rmsHigh > 0) ? peakHigh / rmsHigh : 0.0f;
        
        logMessage("Low Drive (0%):  RMS=" + juce::String(rmsLow, 4) + 
                   ", Peak=" + juce::String(peakLow, 4) + 
                   ", Crest=" + juce::String(crestLow, 3));
        logMessage("High Drive (100%): RMS=" + juce::String(rmsHigh, 4) + 
                   ", Peak=" + juce::String(peakHigh, 4) + 
                   ", Crest=" + juce::String(crestHigh, 3));
        logMessage("");
        logMessage("Crest Factor Change: " + juce::String(crestLow - crestHigh, 3) + 
                   " (should be > 0.2 for aggressive saturation)");
        logMessage("RMS Change: " + juce::String((rmsHigh - rmsLow) / rmsLow * 100.0f, 1) + 
                   "% (may be negative due to compensation)");
        
        // Проверяем, действительно ли сатурация работает
        bool crestDropped = crestHigh < crestLow;
        bool rmsChanged = std::abs(rmsHigh - rmsLow) > 0.01f;
        
        logMessage("");
        logMessage("Crest Factor dropped: " + juce::String(crestDropped ? "YES" : "NO"));
        logMessage("RMS changed: " + juce::String(rmsChanged ? "YES" : "NO"));
        
        if (!crestDropped)
        {
            logMessage("⚠️  WARNING: Crest Factor did not drop! Saturation may not be aggressive enough.");
        }
        
        if (!rmsChanged)
        {
            logMessage("⚠️  WARNING: RMS did not change! Compensation may be too strong.");
        }
    }
};

static SaturationDiagnostics saturationDiag;

