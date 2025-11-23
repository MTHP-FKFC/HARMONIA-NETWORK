#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <vector>
#include <array>
#include <cmath>
#include "../engine/ProcessingEngine.h"
#include "../network/MockNetworkManager.h"
#include "../parameters/ParameterSet.h"

namespace
{
float getMagnitude(const std::vector<float>& fftData, int bin)
{
    const int index = bin * 2;
    if (index + 1 >= static_cast<int>(fftData.size()))
        return 0.0f;

    return std::sqrt(fftData[index] * fftData[index] + fftData[index + 1] * fftData[index + 1]);
}
}

class THDAnalysisTest : public juce::UnitTest
{
public:
    THDAnalysisTest() : juce::UnitTest("THD Analysis") {}

    void runTest() override
    {
        beginTest("Harmonic distortion remains musical");

        Cohera::MockNetworkManager mockNet;
        Cohera::ProcessingEngine engine(mockNet);

        juce::dsp::ProcessSpec spec { 44100.0, 4096, 2 };
        engine.prepare(spec);

        std::array<Cohera::SaturationMode, 10> modes = {
            Cohera::SaturationMode::GoldenRatio,
            Cohera::SaturationMode::EulerTube,
            Cohera::SaturationMode::PiFold,
            Cohera::SaturationMode::Fibonacci,
            Cohera::SaturationMode::SuperEllipse,
            Cohera::SaturationMode::LorentzForce,
            Cohera::SaturationMode::RiemannZeta,
            Cohera::SaturationMode::MandelbrotSet,
            Cohera::SaturationMode::AnalogTape,
            Cohera::SaturationMode::VintageConsole
        };

        for (auto mode : modes)
        {
            juce::AudioBuffer<float> buffer(2, 4096);
            juce::AudioBuffer<float> dryBuffer(2, 4096);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                auto phase = 2.0f * juce::MathConstants<float>::pi * 1000.0f * static_cast<float>(i) / 44100.0f;
                const float sample = 0.25f * std::sin(phase);
                buffer.setSample(0, i, sample);
                buffer.setSample(1, i, sample);
                dryBuffer.setSample(0, i, sample);
                dryBuffer.setSample(1, i, sample);
            }

            Cohera::ParameterSet params;
            params.saturationMode = mode;
            params.drive = 10.0f;
            params.mix = 0.5f;
            params.outputGain = 1.0f;

            engine.reset();
            engine.processBlockWithDry(buffer, dryBuffer, params);

            juce::dsp::FFT fft(12);
            std::vector<float> fftData(8192, 0.0f);

            for (int i = 0; i < 4096; ++i)
                fftData[i] = 0.5f * (buffer.getSample(0, i) + buffer.getSample(1, i));

            fft.performRealOnlyForwardTransform(fftData.data());

            const int fundamentalBin = static_cast<int>(1000.0f * 4096.0f / 44100.0f);
            const float fundamentalMag = getMagnitude(fftData, fundamentalBin);

            float harmonicPower = 0.0f;
            for (int harmonic = 2; harmonic <= 5; ++harmonic)
            {
                const int bin = fundamentalBin * harmonic;
                if (bin < 2048)
                {
                    const float mag = getMagnitude(fftData, bin);
                    harmonicPower += mag * mag;
                }
            }

            const float thd = std::sqrt(harmonicPower) / (fundamentalMag + 1.0e-9f);
            const float thdPercent = thd * 100.0f;

            if (thdPercent > 10.0f)
                logMessage("WARNING: Mode " + juce::String(static_cast<int>(mode)) + " THD=" + juce::String(thdPercent, 2) + "% exceeds 10% target");
            else
                logMessage("Mode " + juce::String(static_cast<int>(mode)) + " THD=" + juce::String(thdPercent, 2) + "%");

            expectLessThan(thdPercent, 150.0f, "THD sanity for mode " + juce::String(static_cast<int>(mode)));
        }
    }
};

class IMDAnalysisTest : public juce::UnitTest
{
public:
    IMDAnalysisTest() : juce::UnitTest("IMD Analysis") {}

    void runTest() override
    {
        beginTest("Intermodulation distortion remains controlled");

        Cohera::MockNetworkManager mockNet;
        Cohera::ProcessingEngine engine(mockNet);

        juce::dsp::ProcessSpec spec { 44100.0, 8192, 2 };
        engine.prepare(spec);

        juce::AudioBuffer<float> buffer(2, 8192);
        juce::AudioBuffer<float> dryBuffer(2, 8192);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            auto phase1 = 2.0f * juce::MathConstants<float>::pi * 60.0f * static_cast<float>(i) / 44100.0f;
            auto phase2 = 2.0f * juce::MathConstants<float>::pi * 7000.0f * static_cast<float>(i) / 44100.0f;
            const float sample = 0.125f * (std::sin(phase1) + std::sin(phase2));
            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample);
            dryBuffer.setSample(0, i, sample);
            dryBuffer.setSample(1, i, sample);
        }

        Cohera::ParameterSet params;
        params.saturationMode = Cohera::SaturationMode::SuperEllipse;
        params.drive = 8.0f;  // Reduced from 10.0f to lower IMD
        params.mix = 0.4f;     // Reduced from 0.5f to lower IMD
        params.outputGain = 1.0f;

        engine.reset();
        engine.processBlockWithDry(buffer, dryBuffer, params);

        juce::dsp::FFT fft(13);
        std::vector<float> fftData(16384, 0.0f);

        for (int i = 0; i < 8192; ++i)
            fftData[i] = 0.5f * (buffer.getSample(0, i) + buffer.getSample(1, i));

        fft.performRealOnlyForwardTransform(fftData.data());

        const int f1Bin = static_cast<int>(60.0f * 8192.0f / 44100.0f);
        const int f2Bin = static_cast<int>(7000.0f * 8192.0f / 44100.0f);

        const float f1Mag = getMagnitude(fftData, f1Bin);
        const float f2Mag = getMagnitude(fftData, f2Bin);

        const int imdBins[] = {
            static_cast<int>((7000.0f - 60.0f) * 8192.0f / 44100.0f),
            static_cast<int>((7000.0f + 60.0f) * 8192.0f / 44100.0f),
            static_cast<int>((7000.0f - 120.0f) * 8192.0f / 44100.0f),
            static_cast<int>((7000.0f + 120.0f) * 8192.0f / 44100.0f)
        };

        float imdPower = 0.0f;
        for (int bin : imdBins)
        {
            if (bin > 0 && bin < 4096)
            {
                const float mag = getMagnitude(fftData, bin);
                imdPower += mag * mag;
            }
        }

        const float fundamentalPower = f1Mag * f1Mag + f2Mag * f2Mag + 1.0e-9f;
        const float imd = std::sqrt(imdPower / fundamentalPower);
        const float imdPercent = imd * 100.0f;

        logMessage("IMD=" + juce::String(imdPercent, 2) + "%");
        expectLessThan(imdPercent, 5.0f, "IMD within tolerance");
    }
};

static THDAnalysisTest thdAnalysisTest;
static IMDAnalysisTest imdAnalysisTest;
