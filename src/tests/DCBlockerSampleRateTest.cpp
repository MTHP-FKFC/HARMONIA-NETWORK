#include <juce_core/juce_core.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <cmath>
#include "../dsp/DCBlocker.h"

class DCBlockerSampleRateTest : public juce::UnitTest
{
public:
    DCBlockerSampleRateTest() : juce::UnitTest("DCBlocker Sample Rate Independence") {}

    void runTest() override
    {
        beginTest("Cutoff consistency across sample rates");

        const double sampleRates[] = { 44100.0, 48000.0, 88200.0, 96000.0, 192000.0 };

        for (double sr : sampleRates)
        {
            DCBlocker blocker;
            blocker.prepare(sr);

            const int numSamples = juce::jmax(4096, static_cast<int>(sr * 0.1));
            juce::AudioBuffer<float> buffer(1, numSamples);
            buffer.clear();

            // Inject constant DC offset
            buffer.applyGain(1.0f);

            for (int i = 0; i < numSamples; ++i)
                buffer.setSample(0, i, blocker.process(buffer.getSample(0, i)));

            auto finalDC = buffer.getSample(0, buffer.getNumSamples() - 1);
            expectLessThan(std::abs(finalDC), 0.01f,
                "DC removal @ " + juce::String(sr) + " Hz");

            blocker.reset();
            buffer.clear();

            const float sineFreq = 20.0f;
            for (int i = 0; i < numSamples; ++i)
            {
                auto phase = 2.0f * juce::MathConstants<float>::pi * sineFreq * static_cast<float>(i) / static_cast<float>(sr);
                buffer.setSample(0, i, std::sin(phase));
            }

            const auto inputRMS = buffer.getRMSLevel(0, 0, numSamples);

            for (int i = 0; i < numSamples; ++i)
                buffer.setSample(0, i, blocker.process(buffer.getSample(0, i)));

            const auto outputRMS = buffer.getRMSLevel(0, 0, numSamples);
            const float attenuation = outputRMS / (inputRMS + 1.0e-6f);

            expectGreaterThan(attenuation, 0.89f,
                "20Hz preserved @ " + juce::String(sr) + " Hz");
        }
    }
};

static DCBlockerSampleRateTest dcBlockerSampleRateTest;
