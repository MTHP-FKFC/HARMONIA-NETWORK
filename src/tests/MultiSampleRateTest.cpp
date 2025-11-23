#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <cmath>
#include "../engine/ProcessingEngine.h"
#include "../network/MockNetworkManager.h"
#include "../parameters/ParameterSet.h"

class MultiSampleRateConsistencyTest : public juce::UnitTest
{
public:
    MultiSampleRateConsistencyTest() : juce::UnitTest("Multi Sample Rate Consistency") {}

    void runTest() override
    {
        beginTest("Output RMS stays consistent across sample rates");

        const std::vector<double> sampleRates = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };
        float referenceDB = 0.0f;

        for (size_t idx = 0; idx < sampleRates.size(); ++idx)
        {
            Cohera::MockNetworkManager mockNet;
            Cohera::ProcessingEngine engine(mockNet);
            const double sampleRate = sampleRates[idx];
            juce::dsp::ProcessSpec spec { sampleRate, 2048, 2 };
            engine.prepare(spec);

            juce::AudioBuffer<float> buffer(2, 2048);
            juce::AudioBuffer<float> dryBuffer(2, 2048);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                auto phase = 2.0f * juce::MathConstants<float>::pi * 250.0f * static_cast<float>(i) / static_cast<float>(sampleRate);
                const float sample = 0.3f * std::sin(phase);
                buffer.setSample(0, i, sample);
                buffer.setSample(1, i, sample);
                dryBuffer.setSample(0, i, sample);
                dryBuffer.setSample(1, i, sample);
            }

            Cohera::ParameterSet params;
            params.drive = 60.0f;
            params.mix = 1.0f;
            params.outputGain = 1.0f;
            params.saturationMode = Cohera::SaturationMode::SuperEllipse;

            engine.reset();
            engine.processBlockWithDry(buffer, dryBuffer, params);

            const float wetRMS = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
            const float dryRMS = dryBuffer.getRMSLevel(0, 0, dryBuffer.getNumSamples());
            const float ratioDB = juce::Decibels::gainToDecibels((wetRMS + 1.0e-6f) / (dryRMS + 1.0e-6f));

            logMessage("SR=" + juce::String(sampleRate, 0) + " Wet/Dry Delta=" + juce::String(ratioDB, 2) + "dB Lat=" + juce::String(engine.getLatency(), 2));

            if (idx == 0)
            {
                referenceDB = ratioDB;
            }
            else
            {
                // Allow slightly higher tolerance for very high sample rates
                float tolerance = (sampleRate >= 96000.0) ? 0.7f : 0.5f;
                expectWithinAbsoluteError(ratioDB, referenceDB, tolerance,
                    "RMS deviation within +/-" + juce::String(tolerance, 1) + "dB for SR=" + juce::String(sampleRate, 0));
            }
        }
    }
};

static MultiSampleRateConsistencyTest multiSampleRateConsistencyTest;
