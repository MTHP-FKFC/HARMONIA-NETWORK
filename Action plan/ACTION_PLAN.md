# Cohera Saturator - Action Plan for Production Perfection
## Приоритизированный план доведения до совершенства аудио-продакшена

**Дата:** 2024  
**Версия плагина:** 1.30  
**Текущий статус:** 87/100 - Production Ready  
**Целевой статус:** 95/100 - Professional Grade

---

## 🎯 EXECUTIVE SUMMARY

**Критичные задачи:** 2  
**Важные задачи:** 4  
**Рекомендуемые задачи:** 6  
**Ожидаемое время:** 2-3 недели (1 full-time engineer)

---

## 🔥 КРИТИЧНЫЕ ЗАДАЧИ (Must Fix Before Release)

### TASK #1: Sample-Rate Independent DCBlocker ⚠️ CRITICAL
**Приоритет:** P0 (BLOCKER)  
**Время:** 2 hours  
**Сложность:** Low  
**Влияние:** HIGH (affects audio quality at 96kHz+)

#### Проблема:
```cpp
// src/dsp/DCBlocker.h - ТЕКУЩИЙ КОД
float process(float input) {
    float y = input - x1 + 0.98f * y1;  // ❌ Cutoff зависит от sample rate!
    x1 = input;
    y1 = y;
    return y;
}
```

**Математика проблемы:**
```
fc = fs * (1 - R) / (2π)

@ 44.1kHz: fc = 44100 * 0.02 / 6.28 ≈ 140 Hz  ✅ OK
@ 96kHz:   fc = 96000 * 0.02 / 6.28 ≈ 306 Hz  ❌ СЛИШКОМ ВЫСОКО! (убирает bass)
```

#### Решение:

**Step 1: Обновить DCBlocker.h**
```cpp
// src/dsp/DCBlocker.h - НОВЫЙ КОД
#pragma once

#include <cmath>
#include <algorithm>
#include <juce_core/juce_core.h>

class DCBlocker
{
public:
    void reset() {
        x1 = 0.0f;
        y1 = 0.0f;
    }
    
    /**
     * @brief Prepare filter for specific sample rate
     * @param sampleRate Target sample rate in Hz
     * 
     * Calculates coefficient R to achieve consistent 5Hz cutoff
     * across all sample rates.
     */
    void prepare(double sampleRate) {
        // Target cutoff: 5 Hz (below human hearing, preserves sub bass)
        const double targetCutoffHz = 5.0;
        
        // Calculate coefficient: R = 1 - (2π * fc / fs)
        R = 1.0 - (2.0 * 3.14159265358979323846 * targetCutoffHz / sampleRate);
        
        // Safety clamp: prevent instability at extreme sample rates
        R = juce::jlimit(0.95, 0.999, R);
        
        juce::Logger::writeToLog(
            "DCBlocker prepared @ " + juce::String(sampleRate, 1) + "Hz: " +
            "R=" + juce::String(R, 6) + 
            ", fc=" + juce::String(sampleRate * (1.0 - R) / (2.0 * 3.14159265358979323846), 2) + "Hz"
        );
    }

    /**
     * @brief Process single sample through DC blocking filter
     * 
     * First-order high-pass filter (IIR):
     * y[n] = x[n] - x[n-1] + R * y[n-1]
     */
    float process(float input) {
        float y = input - x1 + R * y1;
        x1 = input;
        y1 = y;
        return y;
    }

private:
    double R = 0.995;  // Default for 44.1kHz, will be recalculated in prepare()
    float x1 = 0.0f;
    float y1 = 0.0f;
};
```

**Step 2: Обновить BandProcessingEngine.h**
```cpp
// src/engine/BandProcessingEngine.h
// В методе prepare():

void prepare(const juce::dsp::ProcessSpec& spec)
{
    transientEngine.prepare(spec);
    analogEngine.prepare(spec);

    // ✅ ADD THIS:
    for(auto& dcb : dcBlockers) {
        dcb.prepare(spec.sampleRate);
    }
}
```

**Step 3: Обновить MixEngine.h**
```cpp
// src/engine/MixEngine.h
// В методе prepare():

void prepare(const juce::dsp::ProcessSpec &spec) {
    dryDelayLine.prepare(spec);
    dryDelayLine.setMaximumDelayInSamples(spec.sampleRate);
    psychoGain.prepare(spec.sampleRate);
    
    // ✅ ADD THIS:
    dcBlockerLeft.prepare(spec.sampleRate);
    dcBlockerRight.prepare(spec.sampleRate);
    
    smoothMix.reset(spec.sampleRate, 0.02);
    smoothGain.reset(spec.sampleRate, 0.02);
    smoothFocus.reset(spec.sampleRate, 0.02);
    
    delayedDryBuffer.setSize(spec.numChannels, spec.maximumBlockSize * 2, false, true, false);
    preparedMaxBlockSize = spec.maximumBlockSize * 2;
}
```

#### Критерии успеха:
- [ ] DCBlocker cutoff = 5Hz ±1Hz на всех sample rates (44.1/48/96/192kHz)
- [ ] No bass loss @ 96kHz при A/B сравнении с 44.1kHz
- [ ] Тесты проходят на всех sample rates
- [ ] Log показывает корректные cutoff frequencies

#### Тестирование:
```cpp
// src/tests/DCBlockerSampleRateTest.cpp (NEW FILE)
class DCBlockerSampleRateTest : public juce::UnitTest {
public:
    DCBlockerSampleRateTest() : juce::UnitTest("DCBlocker Sample Rate Independence") {}
    
    void runTest() override {
        beginTest("Cutoff frequency consistent across sample rates");
        
        double sampleRates[] = {44100.0, 48000.0, 88200.0, 96000.0, 192000.0};
        
        for (double sr : sampleRates) {
            DCBlocker blocker;
            blocker.prepare(sr);
            
            // Generate DC offset signal (1.0)
            juce::AudioBuffer<float> buffer(1, 4096);
            buffer.clear();
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                buffer.setSample(0, i, 1.0f);
            }
            
            // Process
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                buffer.setSample(0, i, blocker.process(buffer.getSample(0, i)));
            }
            
            // After ~4096 samples, DC should be < 0.01
            float finalDC = buffer.getSample(0, buffer.getNumSamples() - 1);
            expectLessThan(std::abs(finalDC), 0.01f, 
                "DCBlocker removes DC @ " + juce::String(sr) + "Hz");
            
            // Generate 20Hz sine (should pass through)
            blocker.reset();
            buffer.clear();
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                float phase = 2.0f * 3.14159f * 20.0f * i / sr;
                buffer.setSample(0, i, std::sin(phase));
            }
            
            float inputRMS = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
            
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                buffer.setSample(0, i, blocker.process(buffer.getSample(0, i)));
            }
            
            float outputRMS = buffer.getRMSLevel(0, 0, buffer.getNumSamples());
            float attenuation = outputRMS / inputRMS;
            
            // 20Hz should have < 1dB attenuation
            expectGreaterThan(attenuation, 0.89f, 
                "20Hz preserved @ " + juce::String(sr) + "Hz (atten=" + juce::String(attenuation, 3) + ")");
        }
    }
};

static DCBlockerSampleRateTest dcBlockerSampleRateTest;
```

---

### TASK #2: Verify ProcessingEngine Integration ✅ VERIFICATION
**Приоритет:** P0 (BLOCKER)  
**Время:** 30 minutes  
**Сложность:** Low  
**Влияние:** HIGH (core functionality)

#### Проблема:
В `ProcessingEngine.h:102` есть комментарий:
```cpp
// ... (код processBlockWithDry остается тем же, что был ранее) ...
```

Но реальный код ЕСТЬ и работает (строки 106-156). Нужно просто убедиться что нет регрессий.

#### Действия:
1. Запустить все существующие тесты
2. Проверить что `PluginProcessor::processBlock()` вызывает `processingEngine.processBlockWithDry()`
3. Проверить latency reporting

#### Команды:
```bash
cd build
make Cohera_Tests -j4
./tests/Cohera_Tests

# Expected output:
# ✅ FilterBankIntegrationTest: PASS
# ✅ FullSystemPhaseTest: PASS
# ✅ NullPhaseInversionTest: PASS
# ✅ All 7/7 tests passing
```

#### Критерии успеха:
- [ ] Все тесты проходят
- [ ] Latency reported = ~93-97 samples @ 44.1kHz
- [ ] Dry/Wet alignment корректен (no comb filtering)
- [ ] No crashes при variable block sizes

---

## 🔶 ВАЖНЫЕ ЗАДАЧИ (Should Fix for Professional Grade)

### TASK #3: THD/IMD Harmonic Analysis Tests 🎵
**Приоритет:** P1 (High)  
**Время:** 1 day  
**Сложность:** Medium  
**Влияние:** MEDIUM (quality assurance)

#### Цель:
Убедиться что каждый Divine Math режим производит музыкальные гармоники, а не harsh artifacts.

#### Создать новый тест файл:

**src/tests/HarmonicAnalysisTest.cpp** (NEW FILE)
```cpp
#include <juce_dsp/juce_dsp.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include "../engine/ProcessingEngine.h"
#include "../network/MockNetworkManager.h"
#include "../parameters/ParameterSet.h"

namespace CoheraTests {

/**
 * @brief THD (Total Harmonic Distortion) Test
 * 
 * Measures harmonic content added by saturation.
 * Acceptable THD: < 5% for musical saturation
 */
class THDTest : public juce::UnitTest {
public:
    THDTest() : juce::UnitTest("THD Analysis") {}
    
    void runTest() override {
        beginTest("Total Harmonic Distortion < 5% for all Divine Math modes");
        
        Cohera::MockNetworkManager mockNet;
        Cohera::ProcessingEngine engine(mockNet);
        
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 4096;
        spec.numChannels = 2;
        engine.prepare(spec);
        
        // Test all 10 Divine Math modes
        Cohera::SaturationMode modes[] = {
            Cohera::SaturationMode::SuperEllipse,
            Cohera::SaturationMode::EulerTube,
            Cohera::SaturationMode::FermatSpiral,
            Cohera::SaturationMode::HyperbolicTan,
            Cohera::SaturationMode::Cassini,
            Cohera::SaturationMode::Lemniscate,
            Cohera::SaturationMode::Sigmoid,
            Cohera::SaturationMode::Algebraic,
            Cohera::SaturationMode::Fractal,
            Cohera::SaturationMode::Transcendental
        };
        
        for (auto mode : modes) {
            // Generate 1kHz sine wave @ -12dBFS
            juce::AudioBuffer<float> buffer(2, 4096);
            juce::AudioBuffer<float> dryBuffer(2, 4096);
            
            for (int i = 0; i < buffer.getNumSamples(); ++i) {
                float phase = 2.0f * 3.14159f * 1000.0f * i / 44100.0f;
                float sample = 0.25f * std::sin(phase); // -12dBFS
                buffer.setSample(0, i, sample);
                buffer.setSample(1, i, sample);
                dryBuffer.setSample(0, i, sample);
                dryBuffer.setSample(1, i, sample);
            }
            
            // Process
            Cohera::ParameterSet params;
            params.mathMode = mode;
            params.drive = 50.0f; // 50% drive
            params.mix = 1.0f;    // 100% wet
            params.outputGain = 1.0f;
            
            engine.reset();
            engine.processBlockWithDry(buffer, dryBuffer, params);
            
            // FFT Analysis
            juce::dsp::FFT fft(12); // 4096 points
            std::vector<float> fftData(8192, 0.0f);
            
            // Copy mono sum
            for (int i = 0; i < 4096; ++i) {
                fftData[i] = (buffer.getSample(0, i) + buffer.getSample(1, i)) * 0.5f;
            }
            
            fft.performRealOnlyForwardTransform(fftData.data());
            
            // Find fundamental (1kHz bin)
            int fundamentalBin = (int)(1000.0f * 4096.0f / 44100.0f);
            float fundamentalMag = std::sqrt(
                fftData[fundamentalBin * 2] * fftData[fundamentalBin * 2] +
                fftData[fundamentalBin * 2 + 1] * fftData[fundamentalBin * 2 + 1]
            );
            
            // Measure harmonics (2kHz, 3kHz, 4kHz, 5kHz)
            float harmonicPower = 0.0f;
            for (int h = 2; h <= 5; ++h) {
                int bin = fundamentalBin * h;
                if (bin < 2048) {
                    float mag = std::sqrt(
                        fftData[bin * 2] * fftData[bin * 2] +
                        fftData[bin * 2 + 1] * fftData[bin * 2 + 1]
                    );
                    harmonicPower += mag * mag;
                }
            }
            
            float thd = std::sqrt(harmonicPower) / fundamentalMag;
            float thdPercent = thd * 100.0f;
            
            logMessage("Mode " + juce::String((int)mode) + ": THD = " + 
                      juce::String(thdPercent, 2) + "%");
            
            // Musical saturation: THD < 5% acceptable
            // Aggressive saturation: THD < 10% acceptable
            expectLessThan(thdPercent, 10.0f, 
                "THD acceptable for mode " + juce::String((int)mode));
        }
    }
};

/**
 * @brief IMD (Intermodulation Distortion) Test
 * 
 * Measures intermodulation products (non-harmonic artifacts).
 * Two-tone test: 60Hz + 7kHz
 * Acceptable IMD: < 1%
 */
class IMDTest : public juce::UnitTest {
public:
    IMDTest() : juce::UnitTest("IMD Analysis") {}
    
    void runTest() override {
        beginTest("Intermodulation Distortion < 1%");
        
        Cohera::MockNetworkManager mockNet;
        Cohera::ProcessingEngine engine(mockNet);
        
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 8192;
        spec.numChannels = 2;
        engine.prepare(spec);
        
        // Generate two-tone signal: 60Hz + 7kHz @ -18dBFS each
        juce::AudioBuffer<float> buffer(2, 8192);
        juce::AudioBuffer<float> dryBuffer(2, 8192);
        
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            float phase1 = 2.0f * 3.14159f * 60.0f * i / 44100.0f;
            float phase2 = 2.0f * 3.14159f * 7000.0f * i / 44100.0f;
            float sample = 0.125f * (std::sin(phase1) + std::sin(phase2));
            buffer.setSample(0, i, sample);
            buffer.setSample(1, i, sample);
            dryBuffer.setSample(0, i, sample);
            dryBuffer.setSample(1, i, sample);
        }
        
        // Process with moderate drive
        Cohera::ParameterSet params;
        params.mathMode = Cohera::SaturationMode::SuperEllipse;
        params.drive = 50.0f;
        params.mix = 1.0f;
        params.outputGain = 1.0f;
        
        engine.reset();
        engine.processBlockWithDry(buffer, dryBuffer, params);
        
        // FFT Analysis
        juce::dsp::FFT fft(13); // 8192 points
        std::vector<float> fftData(16384, 0.0f);
        
        for (int i = 0; i < 8192; ++i) {
            fftData[i] = (buffer.getSample(0, i) + buffer.getSample(1, i)) * 0.5f;
        }
        
        fft.performRealOnlyForwardTransform(fftData.data());
        
        // Find fundamental bins
        int f1Bin = (int)(60.0f * 8192.0f / 44100.0f);
        int f2Bin = (int)(7000.0f * 8192.0f / 44100.0f);
        
        float f1Mag = getMagnitude(fftData, f1Bin);
        float f2Mag = getMagnitude(fftData, f2Bin);
        
        // Measure intermodulation products (f2 ± f1, f2 ± 2*f1, etc.)
        float imdPower = 0.0f;
        int imdBins[] = {
            (int)((7000.0f - 60.0f) * 8192.0f / 44100.0f),
            (int)((7000.0f + 60.0f) * 8192.0f / 44100.0f),
            (int)((7000.0f - 120.0f) * 8192.0f / 44100.0f),
            (int)((7000.0f + 120.0f) * 8192.0f / 44100.0f)
        };
        
        for (int bin : imdBins) {
            if (bin > 0 && bin < 4096) {
                float mag = getMagnitude(fftData, bin);
                imdPower += mag * mag;
            }
        }
        
        float fundamentalPower = f1Mag * f1Mag + f2Mag * f2Mag;
        float imd = std::sqrt(imdPower / fundamentalPower);
        float imdPercent = imd * 100.0f;
        
        logMessage("IMD = " + juce::String(imdPercent, 3) + "%");
        
        // IMD < 1% for clean saturation
        expectLessThan(imdPercent, 1.5f, "IMD acceptable");
    }
    
private:
    float getMagnitude(const std::vector<float>& fftData, int bin) {
        return std::sqrt(
            fftData[bin * 2] * fftData[bin * 2] +
            fftData[bin * 2 + 1] * fftData[bin * 2 + 1]
        );
    }
};

static THDTest thdTest;
static IMDTest imdTest;

} // namespace CoheraTests
```

#### Критерии успеха:
- [ ] THD < 10% для всех Divine Math modes при drive=50%
- [ ] IMD < 1.5% для dual-tone test
- [ ] No NaN в FFT output
- [ ] Log показывает THD values для каждого режима

---

### TASK #4: Frequency Balance Calibration (kBandTilt) 🎚️
**Приоритет:** P1 (High)  
**Время:** 4 hours  
**Сложность:** Medium  
**Влияние:** MEDIUM (psychoacoustic accuracy)

#### Проблема:
Текущие kBandTilt коэффициенты не верифицированы психоакустически:
```cpp
// FilterBankEngine.h:114
constexpr float kBandTilt[6] = {0.5f, 0.75f, 1.0f, 1.0f, 1.1f, 1.25f};
```

#### Цель:
Откалибровать коэффициенты так, чтобы спектральный баланс соответствовал Equal Loudness Contours (ISO 226).

#### Создать тест:

**src/tests/FrequencyBalanceTest.cpp** (NEW FILE)
```cpp
#include <juce_dsp/juce_dsp.h>
#include "../engine/ProcessingEngine.h"
#include "../network/MockNetworkManager.h"

namespace CoheraTests {

class FrequencyBalanceTest : public juce:UnitTest {
public:
    FrequencyBalanceTest() : juce::UnitTest("Frequency Balance Calibration") {}
    
    void runTest() override {
        beginTest("Pink noise spectral balance");
        
        // Setup
        Cohera::MockNetworkManager mockNet;
        Cohera::ProcessingEngine engine(mockNet);
        
        juce::dsp::ProcessSpec spec;
        spec.sampleRate = 44100.0;
        spec.maximumBlockSize = 8192;
        spec.numChannels = 2;
        engine.prepare(spec);
        
        // Generate pink noise (equal energy per octave)
        juce::Random random;
        juce::AudioBuffer<float> buffer(2, 8192);
        juce::AudioBuffer<float> dryBuffer(2, 8192);
        
        // Pink noise filter (simple approximation)
        float b0 = 0.99886f, b1 = -1.99754f, b2 = 0.99869f;
        float a1 = -1.99754f, a2 = 0.99755f;
        float x1 = 0.0f, x2 = 0.0f, y1 = 0.0f, y2 = 0.0f;
        
        for (int i = 0; i < buffer.getNumSamples(); ++i) {
            float white = random.nextFloat() * 2.0f - 1.0f;
            float pink = b0 * white + b1 * x1 + b2 * x2 - a1 * y1 - a2 * y2;
            x2 = x1; x1 = white;
            y2 = y1; y1 = pink;
            
            pink *= 0.1f; // Scale to reasonable level
            buffer.setSample(0, i, pink);
            buffer.setSample(1, i, pink);
            dryBuffer.setSample(0, i, pink);
            dryBuffer.setSample(1, i, pink);
        }
        
        // Measure input spectrum
        auto inputSpectrum = measureSpectrum(dryBuffer);
        
        // Process
        Cohera::ParameterSet params;
        params.drive = 50.0f;
        params.mix = 1.0f;
        params.outputGain = 1.0f;
        
        engine.reset();
        engine.processBlockWithDry(buffer, dryBuffer, params);
        
        // Measure output spectrum
        auto outputSpectrum = measureSpectrum(buffer);
        
        // Compare bands (should be within ±3dB per octave)
        const char* bandNames[] = {"Sub", "Low", "Low-Mid", "Mid", "High-Mid", "High"};
        float freqRanges[][2] = {
            {20, 80}, {80, 250}, {250, 800}, {800, 2500}, {2500, 8000}, {8000, 20000}
        };
        
        for (int b = 0; b < 6; ++b) {
            float inputLevel = getBandLevel(inputSpectrum, freqRanges[b][0], freqRanges[b][1]);
            float outputLevel = getBandLevel(outputSpectrum, freqRanges[b][0], freqRanges[b][1]);
            
            float deltaDB = 20.0f * std::log10(outputLevel / inputLevel);
            
            logMessage(juce::String(bandNames[b]) + ": " + 
                      juce::String(deltaDB, 2) + "dB");
            
            // Allow ±3dB variation (psychoacoustic tolerance)
            expectWithinAbsoluteError(deltaDB, 0.0f, 3.0f, 
                "Band " + juce::String(bandNames[b]) + " balance");
        }
    }
    
private:
    std::vector<float> measureSpectrum(const juce::AudioBuffer<float>& buffer) {
        juce::dsp::FFT fft(13); // 8192 points
        std::vector<float> fftData(16384, 0.0f);
        
        for (int i = 0; i < 8192; ++i) {
            fftData[i] = (buffer.getSample(0, i) + buffer.getSample(1, i)) * 0.5f;
        }
        
        fft.performRealOnlyForwardTransform(fftData.data());
        
        std::vector<float> magnitude(4096);
        for (int i = 0; i < 4096; ++i) {
            magnitude[i] = std::sqrt(
                fftData[i * 2] * fftData[i * 2] +
                fftData[i * 2 + 1] * fftData[i * 2 + 1]
            );
        }
        
        return magnitude;
    }
    
    float getBandLevel(const std::vector<float>& spectrum, float fLow, float fHigh) {
        int binLow = (int)(fLow * 8192.0f / 44100.0f);
        int binHigh = (int)(fHigh * 8192.0f / 44100.0f);
        
        float sum = 0.0f;
        for (int i = binLow; i <= binHigh && i < spectrum.size(); ++i) {
            sum += spectrum[i] * spectrum[i];
        }
        
        return std::sqrt(sum / (binHigh - binLow + 1));
    }
};

static FrequencyBalanceTest freqBalanceTest;

} // namespace CoheraTests
```

#### После теста - откалибровать коэффициенты:

**Если тест показывает дисбаланс**, обновить `FilterBankEngine.h`:
```cpp
// Рекомендованные значения на основе ISO 226 @ 80dB SPL:
constexpr float kBandTilt[6] = {
    0.45f,  // Sub: -6.9dB (bass overload protection)
    0.70f,  // Low: -3.1dB
    1.00f,  // Low-Mid: 0dB (reference)
    1.05f,  // Mid: +0.4dB
    1.20f,  // High-Mid: +1.6dB
    1.40f   // High: +2.9dB (compensate ear roll-off)
};
```

#### Критерии успеха:
- [ ] Pink noise test: все полосы в пределах ±3dB
- [ ] A/B listening test: processed pink noise звучит естественно
- [ ] No perceived boost/cut в любой полосе
- [ ] Документировать финальные коэффициенты в коде

---

### TASK #5: Multi-Sample-Rate Testing Suite 📊
**Приоритет:** P1 (High)  
**Время:** 3 hours  
**Сложность:** Low  
**Влияние:** MEDIUM (quality assurance)

#### Цель:
Верифицировать что плагин звучит идентично на всех sample rates.

#### Создать тест:

**src/tests/MultiSampleRateTest.cpp** (NEW FILE)
```cpp
#include <juce_dsp/juce_dsp.h>
#include "../engine/ProcessingEngine.h"
#include "../network/MockNetworkManager.h"

namespace CoheraTests {

class MultiSampleRateTest : public juce::UnitTest {
public:
    MultiSampleRateTest() : juce::UnitTest