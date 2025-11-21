#include <juce_core/juce_core.h>
#include "TestHelpers.h"
#include "../engine/ProcessingEngine.h"
#include "../parameters/ParameterSet.h"

class NullPhaseInversionTest : public juce::UnitTest
{
public:
    NullPhaseInversionTest() : juce::UnitTest("Null Phase Inversion Test") {}

    void runTest() override
    {
        beginTest("Noise nulling with inverted Wet at 50% mix");
        {
            Cohera::ProcessingEngine engine;
            double sr = 48000.0; // Use 48k for consistent MixEngine normalization
            juce::dsp::ProcessSpec spec { sr, 1024, 2 };
            engine.prepare(spec);

            // Diagnostic: print reported engine latency after prepare
            std::cerr << "[TestDiag] engine.getLatency() after prepare = " << engine.getLatency() << " samples\n";

            // Create a pseudo-random noise buffer
            juce::AudioBuffer<float> orig(2, 1024);
            orig.clear();
            std::mt19937 rng(12345);
            std::uniform_real_distribution<float> dist(-0.5f, 0.5f);
            for (int i = 0; i < orig.getNumSamples(); ++i)
                for (int ch = 0; ch < orig.getNumChannels(); ++ch)
                    orig.setSample(ch, i, dist(rng));

            // Dry reference
            juce::AudioBuffer<float> dry;
            dry.makeCopyOf(orig);

            // 1) Produce Wet-only buffer (mix=1.0, drive=0)
            juce::AudioBuffer<float> wetOnly;
            wetOnly.makeCopyOf(orig);
            Cohera::ParameterSet params;
            params.drive = 0.0f;
            params.mix = 1.0f;
            params.outputGain = 1.0f;

            engine.processBlockWithDry(wetOnly, dry, params);

            // 2) Produce delayed Dry buffer as MixEngine would see it (mix=0.0)
            juce::AudioBuffer<float> delayedDry;
            delayedDry.makeCopyOf(orig);
            params.mix = 0.0f;
            engine.processBlockWithDry(delayedDry, dry, params);

            // 3) Construct manual mix: 0.5*delayedDry + 0.5*(inverted wet)
            juce::AudioBuffer<float> manualMix(2, orig.getNumSamples());
            manualMix.clear();
            const float mixAmount = 0.5f;

            for (int ch = 0; ch < manualMix.getNumChannels(); ++ch)
            {
                for (int i = 0; i < manualMix.getNumSamples(); ++i)
                {
                    float d = delayedDry.getSample(ch, i);
                    float w = wetOnly.getSample(ch, i);
                    float out = d * (1.0f - mixAmount) + (-w) * mixAmount; // invert wet
                    manualMix.setSample(ch, i, out);
                }
            }

            // Metrics: RMS magnitude per channel
            float mag0 = manualMix.getMagnitude(0, manualMix.getNumSamples());
            float mag1 = manualMix.getMagnitude(1, manualMix.getNumSamples());
            float maxMag = juce::jmax(mag0, mag1);

            // Expect near-silence (tolerance depends on filter latency rounding)
            // Allow small residual energy: threshold = 1e-3
            float threshold = 1e-3f;
            expect(maxMag < threshold, "Manual mix after inversion should be near-silent; maxMag=" + juce::String(maxMag));

            // For debugging, also compute full-process mix (mix=0.5) and compare
            juce::AudioBuffer<float> fullMix;
            fullMix.makeCopyOf(orig);
            params.mix = 0.5f;
            engine.processBlockWithDry(fullMix, dry, params);

            // Compute difference between engine mix and manualMix: should be small
            bool equalish = CoheraTests::areBuffersEqual(fullMix, manualMix, 1e-2f); // allow looser tolerance
            expect(equalish, "Engine mix vs manual inverted mix should match closely (within tolerance)");
        }
    }
};

static NullPhaseInversionTest nullPhaseInversionTest;
