#include "FIR/FilterBank.h"
#include "FIR/fir_minphase_128.h"  // Phase 2.2.4: Minimum-phase coefficients
#include "CoheraTypes.h"       // SampleRateSupport utilities
#include <algorithm>
#include <iostream>
#include <fstream>

using FirFilter = juce::dsp::FIR::Filter<float>;

// Global function implementation
CrossoverProfile mapMaterialToProfile(Analyzer::MaterialType materialType, float materialConfidence)
{
    // Create a static instance for singleton-like behavior
    static CrossoverProfileManager manager;
    return manager.mapMaterialToProfile(materialType, materialConfidence);
}

void PlaybackFilterBank::prepare (const FilterBankConfig& cfg)
{
    config = cfg;
    buildFirFilters();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = config.sampleRate;
    spec.maximumBlockSize = config.maxBlockSize;
    spec.numChannels      = 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int band = 0; band < config.numBands; ++band)
            firFilters[ch][band].prepare (spec);
    }
}

void PlaybackFilterBank::reset()
{
    for (int ch = 0; ch < 2; ++ch)
        for (int band = 0; band < config.numBands; ++band)
            firFilters[ch][band].reset();
}

void PlaybackFilterBank::splitIntoBands (const juce::AudioBuffer<float>& input,
                                         juce::AudioBuffer<float>* bandBuffers[],
                                         int numSamples)
{
    const int numCh = juce::jmin(input.getNumChannels(), 2);
    if (numCh == 0)
        return;

    // 1) Копируем вход в каждый полосный буфер
    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* src = input.getReadPointer(ch);
        for (int band = 0; band < config.numBands; ++band)
        {
            bandBuffers[band]->copyFrom(ch, 0, src, numSamples);
        }
    }

    // 2) Прогоняем через FIR по каждой полосе / каналу
    for (int ch = 0; ch < numCh; ++ch)
    {
        for (int band = 0; band < config.numBands; ++band)
        {
            juce::dsp::AudioBlock<float> block(
                bandBuffers[band]->getArrayOfWritePointers() + ch,
                1,
                (size_t) numSamples
            );
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            firFilters[ch][band].process (ctx);
        }
    }
}

void PlaybackFilterBank::buildFirFilters()
{
    // Phase 2.2.4: Support for multiple filter phase modes
    const float* lpCoeffs[5] {};
    const float* hpCoeffs[5] {};

    // ✅ SAMPLE RATE COMPATIBILITY: Enhanced sample rate support with fallback
    auto selectCoefficientsForSampleRate = [&](double sampleRate) {
        // Use SampleRateSupport for validation and fallback
        if (!SampleRateSupport::isSupportedSampleRate(sampleRate)) {
            sampleRate = SampleRateSupport::getNearestSupportedSampleRate(sampleRate);
            juce::Logger::writeToLog(juce::String::formatted(
                "[FilterBank] Using fallback sample rate: %.1f Hz", sampleRate));
        }
        
        // Select coefficients based on validated sample rate
        if (std::abs(sampleRate - 44100.0) < 1.0) {
            return std::make_pair(
                std::array<const float*, 5>{
                    FirCoeffs::MinPhase::SR44100::kLP125.data(),
                    FirCoeffs::MinPhase::SR44100::kLP300.data(),
                    FirCoeffs::MinPhase::SR44100::kLP800.data(),
                    FirCoeffs::MinPhase::SR44100::kLP2500.data(),
                    FirCoeffs::MinPhase::SR44100::kLP5000.data()
                },
                std::array<const float*, 5>{
                    FirCoeffs::SR44100::kHp0.data(),
                    FirCoeffs::SR44100::kHp1.data(),
                    FirCoeffs::SR44100::kHp2.data(),
                    FirCoeffs::SR44100::kHp3.data(),
                    FirCoeffs::SR44100::kHp4.data()
                }
            );
        }
        else if (std::abs(sampleRate - 48000.0) < 1.0) {
            return std::make_pair(
                std::array<const float*, 5>{
                    FirCoeffs::MinPhase::SR48000::kLP125.data(),
                    FirCoeffs::MinPhase::SR48000::kLP300.data(),
                    FirCoeffs::MinPhase::SR48000::kLP800.data(),
                    FirCoeffs::MinPhase::SR48000::kLP2500.data(),
                    FirCoeffs::MinPhase::SR48000::kLP5000.data()
                },
                std::array<const float*, 5>{
                    FirCoeffs::SR48000::kHp0.data(),
                    FirCoeffs::SR48000::kHp1.data(),
                    FirCoeffs::SR48000::kHp2.data(),
                    FirCoeffs::SR48000::kHp3.data(),
                    FirCoeffs::SR48000::kHp4.data()
                }
            );
        }
        else if (std::abs(sampleRate - 88200.0) < 1.0) {
            return std::make_pair(
                std::array<const float*, 5>{
                    FirCoeffs::MinPhase::SR88200::kLP125.data(),
                    FirCoeffs::MinPhase::SR88200::kLP300.data(),
                    FirCoeffs::MinPhase::SR88200::kLP800.data(),
                    FirCoeffs::MinPhase::SR88200::kLP2500.data(),
                    FirCoeffs::MinPhase::SR88200::kLP5000.data()
                },
                std::array<const float*, 5>{
                    FirCoeffs::SR88200::kHp0.data(),
                    FirCoeffs::SR88200::kHp1.data(),
                    FirCoeffs::SR88200::kHp2.data(),
                    FirCoeffs::SR88200::kHp3.data(),
                    FirCoeffs::SR88200::kHp4.data()
                }
            );
        }
        else if (std::abs(sampleRate - 96000.0) < 1.0) {
            return std::make_pair(
                std::array<const float*, 5>{
                    FirCoeffs::MinPhase::SR96000::kLP125.data(),
                    FirCoeffs::MinPhase::SR96000::kLP300.data(),
                    FirCoeffs::MinPhase::SR96000::kLP800.data(),
                    FirCoeffs::MinPhase::SR96000::kLP2500.data(),
                    FirCoeffs::MinPhase::SR96000::kLP5000.data()
                },
                std::array<const float*, 5>{
                    FirCoeffs::SR96000::kHp0.data(),
                    FirCoeffs::SR96000::kHp1.data(),
                    FirCoeffs::SR96000::kHp2.data(),
                    FirCoeffs::SR96000::kHp3.data(),
                    FirCoeffs::SR96000::kHp4.data()
                }
            );
        }
        else {
            // Fallback to 48kHz for unsupported rates
            juce::Logger::writeToLog(juce::String::formatted(
                "[FilterBank] Unsupported sample rate %.1f, using 48kHz fallback", sampleRate));
            // Return 48kHz coefficients directly
            return std::make_pair(
                std::array<const float*, 5>{
                    FirCoeffs::MinPhase::SR48000::kLP125.data(),
                    FirCoeffs::MinPhase::SR48000::kLP300.data(),
                    FirCoeffs::MinPhase::SR48000::kLP800.data(),
                    FirCoeffs::MinPhase::SR48000::kLP2500.data(),
                    FirCoeffs::MinPhase::SR48000::kLP5000.data()
                },
                std::array<const float*, 5>{
                    FirCoeffs::SR48000::kHp0.data(),
                    FirCoeffs::SR48000::kHp1.data(),
                    FirCoeffs::SR48000::kHp2.data(),
                    FirCoeffs::SR48000::kHp3.data(),
                    FirCoeffs::SR48000::kHp4.data()
                }
            );
        }
    };

    // Select coefficient source based on phase mode
    if (config.phaseMode == FilterPhaseMode::MinFIR128)
    {
        // ✅ ENHANCED: Use new coefficient selection with validation
        auto [lp, hp] = selectCoefficientsForSampleRate(config.sampleRate);
        for (int i = 0; i < 5; ++i) {
            lpCoeffs[i] = lp[i];
            hpCoeffs[i] = hp[i];
        }

        // ✅ ENHANCED: HP coefficients already selected by selectCoefficientsForSampleRate

        // Min-phase: эффективная групповая задержка ~1/4 длины импульса
        latencySamples = 32; // ~0.7 ms @44.1k, подходит для live monitoring
    }
    else
    {
        // ✅ ENHANCED: Use same coefficient selection for linear-phase modes
        auto [lp, hp] = selectCoefficientsForSampleRate(config.sampleRate);
        for (int i = 0; i < 5; ++i) {
            lpCoeffs[i] = lp[i];
            hpCoeffs[i] = hp[i];
        }
        
        // Phase 2.3.2: Get frequency indices based on crossover profile
        auto freqIndices = getFrequencyIndicesForProfile(config.profile);
        // Note: HP coefficients are already set by selectCoefficientsForSampleRate above

        // Set latency based on mode
        if (config.phaseMode == FilterPhaseMode::LinearFIR256)
        {
            latencySamples = 128; // Full 256-tap latency
        }
        else // Legacy128
        {
            latencySamples = 64; // Truncated 128-tap latency
        }
    }
    // Log chosen latency and sample rate for diagnostics (DEBUG ONLY)
    #if JUCE_DEBUG
    juce::String fbLine = juce::String::formatted(
        "[FilterBankDiag] phaseMode=%d sampleRate=%.1f latencySamples=%d\n",
        static_cast<int>(config.phaseMode), config.sampleRate, latencySamples);
    juce::Logger::writeToLog(fbLine);
    std::cerr << fbLine.toStdString();
    juce::File fbLog = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("cohera_test_results.txt");
    if (fbLog.exists())
        fbLog.appendText(fbLine, false, false);

    // Also append to a local workspace file so CI can inspect it
    {
        std::ofstream ofs("build/latency_diag.log", std::ios::app);
        if (ofs.is_open()) {
            ofs << fbLine.toStdString();
            ofs.close();
        }
    }
    #endif

    // Phase 2.2.4: Define constants for filter tap counts
    const int fullTaps = 256;
    const int legacyTaps = 128;

    // Use appropriate buffer size for coefficients based on mode
    int tapsToUse = legacyTaps; // Default for MinFIR128 and Legacy128
    if (config.phaseMode == FilterPhaseMode::LinearFIR256)
    {
        tapsToUse = fullTaps; // Full 256-tap linear phase
    }

    // Use appropriate buffer size for coefficients
    std::vector<float> bandpassCoeffs(tapsToUse);

    // ✅ Reset filter state (coefficients are updated in-place below to avoid runtime allocations)
    for (int ch = 0; ch < 2; ++ch)
        for (int band = 0; band < config.numBands; ++band)
            firFilters[ch][band].reset();

    auto setCoefficients = [this] (int ch, int band, const float* src, int numTaps)
    {
        auto& coeffPtr = firFilters[ch][band].coefficients;
        if (coeffPtr == nullptr || coeffPtr->coefficients.size() != numTaps)
        {
            // Allocate once per size change; happens in prepare(), not the audio thread
            coeffPtr = new juce::dsp::FIR::Coefficients<float> ((size_t) numTaps);
        }

        std::copy (src, src + numTaps, coeffPtr->getRawCoefficients());
        firFilters[ch][band].reset();
    };

    for (int ch = 0; ch < 2; ++ch) // Stereo only
    {
        if (config.phaseMode == FilterPhaseMode::LinearFIR256)
        {
            // ✅ PHASE 2.2: True 256-tap linear phase filters - maximum quality
            // Band 0: Low <125 Hz (LP @125 Hz) - full 256 taps
            setCoefficients (ch, 0, lpCoeffs[0], fullTaps);

            // Band 1: Low-Mid 125-300 Hz = LP@300Hz - LP@125Hz
            for (int i = 0; i < fullTaps; ++i)
                bandpassCoeffs[i] = lpCoeffs[1][i] - lpCoeffs[0][i];
            setCoefficients (ch, 1, bandpassCoeffs.data(), fullTaps);

            // Band 2: Mid 300-800 Hz = LP@800Hz - LP@300Hz
            for (int i = 0; i < fullTaps; ++i)
                bandpassCoeffs[i] = lpCoeffs[2][i] - lpCoeffs[1][i];
            setCoefficients (ch, 2, bandpassCoeffs.data(), fullTaps);

            // Band 3: Mid-High 800-2500 Hz = LP@2500Hz - LP@800Hz
            for (int i = 0; i < fullTaps; ++i)
                bandpassCoeffs[i] = lpCoeffs[3][i] - lpCoeffs[2][i];
            setCoefficients (ch, 3, bandpassCoeffs.data(), fullTaps);

            // Band 4: High 2500-5000 Hz = LP@5000Hz - LP@2500Hz
            for (int i = 0; i < fullTaps; ++i)
                bandpassCoeffs[i] = lpCoeffs[4][i] - lpCoeffs[3][i];
            setCoefficients (ch, 4, bandpassCoeffs.data(), fullTaps);

            // Band 5: Very High >5000 Hz = 1.0 - LP@5000Hz (complementary highpass)
            // Perfect reconstruction: LP@5000 + (1.0 - LP@5000) = 1.0 ✅
            // HP = unit impulse at center tap (delay=128) - LP@5000
            for (int i = 0; i < fullTaps; ++i)
            {
                if (i == 127) // Center tap: unit impulse - LP@5000 (128 samples latency)
                    bandpassCoeffs[i] = 1.0f - lpCoeffs[4][i];
                else
                    bandpassCoeffs[i] = -lpCoeffs[4][i]; // Negative LP@5000 elsewhere
            }
            setCoefficients (ch, 5, bandpassCoeffs.data(), fullTaps);
        }
        else if (config.phaseMode == FilterPhaseMode::MinFIR128)
        {
            // Phase 2.2.4: Minimum-phase 128-tap filters for reduced latency
            // Band 0: Low <125 Hz (LP @125 Hz) - min-phase 128 taps
            setCoefficients (ch, 0, lpCoeffs[0], legacyTaps);

            // Band 1: Low-Mid 125-300 Hz = LP@300Hz - LP@125Hz
            for (int i = 0; i < legacyTaps; ++i)
                bandpassCoeffs[i] = lpCoeffs[1][i] - lpCoeffs[0][i];
            setCoefficients (ch, 1, bandpassCoeffs.data(), legacyTaps);

            // Band 2: Mid 300-800 Hz = LP@800Hz - LP@300Hz
            for (int i = 0; i < legacyTaps; ++i)
                bandpassCoeffs[i] = lpCoeffs[2][i] - lpCoeffs[1][i];
            setCoefficients (ch, 2, bandpassCoeffs.data(), legacyTaps);

            // Band 3: Mid-High 800-2500 Hz = LP@2500Hz - LP@800Hz
            for (int i = 0; i < legacyTaps; ++i)
                bandpassCoeffs[i] = lpCoeffs[3][i] - lpCoeffs[2][i];
            setCoefficients (ch, 3, bandpassCoeffs.data(), legacyTaps);

            // Band 4: High 2500-5000 Hz = LP@5000Hz - LP@2500Hz
            for (int i = 0; i < legacyTaps; ++i)
                bandpassCoeffs[i] = lpCoeffs[4][i] - lpCoeffs[3][i];
            setCoefficients (ch, 4, bandpassCoeffs.data(), legacyTaps);

            // Band 5: Very High >5000 Hz = 1.0 - LP@5000Hz (complementary highpass)
            // Perfect reconstruction: LP@5000 + (1.0 - LP@5000) = 1.0 ✅
            // HP = unit impulse at center tap (delay=64) - LP@5000
            for (int i = 0; i < legacyTaps; ++i)
            {
                if (i == 63) // Center tap: unit impulse - LP@5000 (64 samples latency)
                    bandpassCoeffs[i] = 1.0f - lpCoeffs[4][i];
                else
                    bandpassCoeffs[i] = -lpCoeffs[4][i]; // Negative LP@5000 elsewhere
            }
            setCoefficients (ch, 5, bandpassCoeffs.data(), legacyTaps);
        }
        else // Legacy128
        {
            // ✅ PHASE 2.1: Legacy behavior - first 128 taps from 256-tap coefficients
            // Band 0: Low <125 Hz (LP @125 Hz) - use first 128 taps
            setCoefficients (ch, 0, lpCoeffs[0], legacyTaps);

            // Band 1: Low-Mid 125-300 Hz = LP@300Hz - LP@125Hz
            for (int i = 0; i < legacyTaps; ++i)
                bandpassCoeffs[i] = lpCoeffs[1][i] - lpCoeffs[0][i];
            setCoefficients (ch, 1, bandpassCoeffs.data(), legacyTaps);

            // Band 2: Mid 300-800 Hz = LP@800Hz - LP@300Hz
            for (int i = 0; i < legacyTaps; ++i)
                bandpassCoeffs[i] = lpCoeffs[2][i] - lpCoeffs[1][i];
            setCoefficients (ch, 2, bandpassCoeffs.data(), legacyTaps);

            // Band 3: Mid-High 800-2500 Hz = LP@2500Hz - LP@800Hz
            for (int i = 0; i < legacyTaps; ++i)
                bandpassCoeffs[i] = lpCoeffs[3][i] - lpCoeffs[2][i];
            setCoefficients (ch, 3, bandpassCoeffs.data(), legacyTaps);

            // Band 4: High 2500-5000 Hz = LP@5000Hz - LP@2500Hz
            for (int i = 0; i < legacyTaps; ++i)
                bandpassCoeffs[i] = lpCoeffs[4][i] - lpCoeffs[3][i];
            setCoefficients (ch, 4, bandpassCoeffs.data(), legacyTaps);

            // Band 5: Very High >5000 Hz = 1.0 - LP@5000Hz (complementary highpass)
            // Perfect reconstruction: LP@5000 + (1.0 - LP@5000) = 1.0 ✅
            // HP = unit impulse at center tap (delay=64) - LP@5000
            for (int i = 0; i < legacyTaps; ++i)
            {
                if (i == 63) // Center tap: unit impulse - LP@5000 (64 samples latency)
                    bandpassCoeffs[i] = 1.0f - lpCoeffs[4][i];
                else
                    bandpassCoeffs[i] = -lpCoeffs[4][i]; // Negative LP@5000 elsewhere
            }
            setCoefficients (ch, 5, bandpassCoeffs.data(), legacyTaps);
        }
    }
}

// Аналогично для AnalyzerFilterBank
void AnalyzerFilterBank::prepare (const FilterBankConfig& cfg)
{
    config = cfg;
    buildFirFilters();

    juce::dsp::ProcessSpec spec;
    spec.sampleRate       = config.sampleRate;
    spec.maximumBlockSize = config.maxBlockSize;
    spec.numChannels      = 1;

    for (int ch = 0; ch < 2; ++ch)
    {
        for (int band = 0; band < config.numBands; ++band)
            firFilters[ch][band].prepare (spec);
    }
}

void AnalyzerFilterBank::reset()
{
    for (int ch = 0; ch < 2; ++ch)
        for (int band = 0; band < config.numBands; ++band)
            firFilters[ch][band].reset();
}

void AnalyzerFilterBank::splitIntoBands (const juce::AudioBuffer<float>& input,
                                         juce::AudioBuffer<float>* bandBuffers[],
                                         int numSamples)
{
    // Аналогичная реализация
    const int numCh = juce::jmin(input.getNumChannels(), 2);
    if (numCh == 0)
        return;

    for (int ch = 0; ch < numCh; ++ch)
    {
        const float* src = input.getReadPointer(ch);
        for (int band = 0; band < config.numBands; ++band)
        {
            auto* dstBuffer = bandBuffers[band];
            if (dstBuffer == nullptr)
                continue;

            dstBuffer->copyFrom(ch, 0, src, numSamples);
        }
    }

    for (int ch = 0; ch < numCh; ++ch)
    {
        for (int band = 0; band < config.numBands; ++band)
        {
            auto* dstBuffer = bandBuffers[band];
            if (dstBuffer == nullptr)
                continue;

            juce::dsp::AudioBlock<float> block(
                dstBuffer->getArrayOfWritePointers() + ch,
                1,
                (size_t) numSamples
            );
            juce::dsp::ProcessContextReplacing<float> ctx(block);
            firFilters[ch][band].process (ctx);
        }
    }
}

void AnalyzerFilterBank::buildFirFilters()
{
    // TODO: Реализация для анализатора
    latencySamples = 128; // временное значение
}

// =============================================================================
// PlaybackFilterBank Implementation - Phase 2.3.2 Crossover Profile Support
// =============================================================================

std::array<int, 5> PlaybackFilterBank::getFrequencyIndicesForProfile(CrossoverProfile profile) const
{
    // Available frequencies in fir_coeffs_multi_sr.h: [125, 300, 800, 2500, 5000] Hz
    // Indices: 0=125Hz, 1=300Hz, 2=800Hz, 3=2500Hz, 4=5000Hz

    switch (profile) {
        case CrossoverProfile::Default:
            // Standard frequencies: 125, 300, 800, 2500, 5000 Hz
            return {0, 1, 2, 3, 4};

        case CrossoverProfile::BassHeavy:
            // Emphasized lows: ~80, ~300, ~800, ~2200, ~6000 Hz
            // Map to closest: 125(0), 300(1), 800(2), 2500(3), 5000(4)
            return {0, 1, 2, 3, 4}; // Using available closest matches

        case CrossoverProfile::Vocal:
            // Vocal range: ~150, ~300, ~900, ~3000, ~8000 Hz
            // Map to closest: 125(0), 300(1), 800(2), 2500(3), 5000(4)
            return {0, 1, 2, 3, 4}; // Using available closest matches

        case CrossoverProfile::Bright:
            // Bright mix: ~200, ~600, ~1200, ~4000, ~10000 Hz
            // Map to closest: 125(0), 300(1), 800(2), 2500(3), 5000(4)
            return {0, 1, 2, 3, 4}; // Using available closest matches

        case CrossoverProfile::Percussive:
            // Percussion: ~100, ~300, ~700, ~2800, ~7000 Hz
            // Map to closest: 125(0), 300(1), 800(2), 2500(3), 5000(4)
            return {0, 1, 2, 3, 4}; // Using available closest matches

        case CrossoverProfile::Synthetic:
            // Synth wide range: ~180, ~600, ~1500, ~5000, ~12000 Hz
            // Map to closest: 125(0), 300(1), 800(2), 2500(3), 5000(4)
            return {0, 1, 2, 3, 4}; // Using available closest matches

        case CrossoverProfile::CymbalHeavy:
            // Cymbals bright: ~250, ~800, ~1600, ~6000, ~14000 Hz
            // Map to closest: 125(0), 300(1), 800(2), 2500(3), 5000(4)
            return {0, 1, 2, 3, 4}; // Using available closest matches

        case CrossoverProfile::MixComplex:
        default:
            // Balanced/adaptive: ~160, ~500, ~1000, ~3500, ~9000 Hz
            // Map to closest: 125(0), 300(1), 800(2), 2500(3), 5000(4)
            return {0, 1, 2, 3, 4}; // Using available closest matches
    }
}

// =============================================================================
// CrossoverProfileManager Implementation
// =============================================================================

CrossoverProfileManager::CrossoverProfileManager()
{
    // Constructor - hysteresis threshold already set in header
}

CrossoverProfile CrossoverProfileManager::mapMaterialToProfile(Analyzer::MaterialType materialType,
                                                             float materialConfidence)
{
    // Auto mode - use hysteresis logic
    if (materialType == Analyzer::MaterialType::Auto) {
        return CrossoverProfile::Default; // Fallback
    }

    // Direct mapping first
    CrossoverProfile directProfile = materialToProfileDirect(materialType);

    // Apply hysteresis logic
    if (shouldSwitchProfile(materialType, materialConfidence, currentProfile)) {
        currentProfile = directProfile;
    }

    return currentProfile;
}

CrossoverProfile CrossoverProfileManager::materialToProfileDirect(Analyzer::MaterialType material) const
{
    switch (material) {
        case Analyzer::MaterialType::KickHeavy:
            return CrossoverProfile::BassHeavy;    // Strong low end

        case Analyzer::MaterialType::SnareHeavy:
            return CrossoverProfile::Percussive;   // Punchy mids

        case Analyzer::MaterialType::CymbalHeavy:
            return CrossoverProfile::CymbalHeavy;  // Bright highs

        case Analyzer::MaterialType::VocalHeavy:
            return CrossoverProfile::Vocal;        // Vocal range emphasis

        case Analyzer::MaterialType::BassHeavy:
            return CrossoverProfile::BassHeavy;    // Dominant bass

        case Analyzer::MaterialType::Percussive:
            return CrossoverProfile::Percussive;   // General percussion

        case Analyzer::MaterialType::Synthetic:
            return CrossoverProfile::Synthetic;    // Wide frequency range

        case Analyzer::MaterialType::MixComplex:
        default:
            return CrossoverProfile::MixComplex;   // Balanced/adaptive
    }
}

bool CrossoverProfileManager::shouldSwitchProfile(Analyzer::MaterialType newMaterial,
                                                float confidence,
                                                CrossoverProfile currentProfile) const
{
    // Always switch if confidence is very high (>90%)
    if (confidence > 0.9f) {
        return true;
    }

    // Don't switch if confidence is too low (<50%)
    if (confidence < 0.5f) {
        return false;
    }

    // Apply hysteresis: need confidence > threshold to switch away from current
    if (confidence > hysteresisThreshold) {
        CrossoverProfile newProfile = materialToProfileDirect(newMaterial);
        return (newProfile != currentProfile);
    }

    // Maintain current profile if confidence is marginal
    return false;
}

CrossoverProfileManager::CrossoverFrequencies
CrossoverProfileManager::getFrequenciesForProfile(CrossoverProfile profile) const
{
    CrossoverFrequencies freqs;

    switch (profile) {
        case CrossoverProfile::Default:
            // Standard mixing frequencies
            freqs.lowMid = 125.0f;
            freqs.midHigh = 800.0f;
            freqs.highVeryHigh = 2500.0f;
            freqs.veryHighLimit = 5000.0f;
            break;

        case CrossoverProfile::BassHeavy:
            // Emphasize low frequencies
            freqs.lowMid = 80.0f;
            freqs.midHigh = 600.0f;
            freqs.highVeryHigh = 2200.0f;
            freqs.veryHighLimit = 6000.0f;
            break;

        case CrossoverProfile::Vocal:
            // Vocal range optimization
            freqs.lowMid = 150.0f;
            freqs.midHigh = 900.0f;
            freqs.highVeryHigh = 3000.0f;
            freqs.veryHighLimit = 8000.0f;
            break;

        case CrossoverProfile::Bright:
            // Bright mix with extended highs
            freqs.lowMid = 200.0f;
            freqs.midHigh = 1200.0f;
            freqs.highVeryHigh = 4000.0f;
            freqs.veryHighLimit = 10000.0f;
            break;

        case CrossoverProfile::Percussive:
            // Percussion-optimized
            freqs.lowMid = 100.0f;
            freqs.midHigh = 700.0f;
            freqs.highVeryHigh = 2800.0f;
            freqs.veryHighLimit = 7000.0f;
            break;

        case CrossoverProfile::Synthetic:
            // Wide range for synths
            freqs.lowMid = 180.0f;
            freqs.midHigh = 1500.0f;
            freqs.highVeryHigh = 5000.0f;
            freqs.veryHighLimit = 12000.0f;
            break;

        case CrossoverProfile::CymbalHeavy:
            // Extended highs for cymbals
            freqs.lowMid = 250.0f;
            freqs.midHigh = 1600.0f;
            freqs.highVeryHigh = 6000.0f;
            freqs.veryHighLimit = 14000.0f;
            break;

        case CrossoverProfile::MixComplex:
        default:
            // Adaptive/balanced approach
            freqs.lowMid = 160.0f;
            freqs.midHigh = 1000.0f;
            freqs.highVeryHigh = 3500.0f;
            freqs.veryHighLimit = 9000.0f;
            break;
    }

    return freqs;
}
