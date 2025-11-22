# 🎧 AUDIO REGRESSION TEST SUITE
**Purpose**: Ensure audio output remains bit-accurate (or mathematically identical) after refactoring.

---

## 📁 DIRECTORY STRUCTURE

```
tests/regression/
├── reference_audio/     # Baseline audio (v1.30 output)
│   ├── sine_440hz_default.wav
│   ├── sine_sweep_extreme_drive.wav
│   ├── white_noise_network.wav
│   ├── kick_full_mojo.wav
│   └── commercial_mix_default.wav
├── test_audio/          # Current version output (generated)
├── presets/             # Test presets (.xml)
│   ├── default.xml
│   ├── extreme_drive.xml
│   ├── network_active.xml
│   └── full_mojo.xml
└── test_audio_regression.cpp  # Test runner
```

---

## 🎯 TEST SIGNALS

### **1. Sine 440Hz (-6dB)**
- **Purpose**: Pure tone, reveals harmonic distortion
- **Duration**: 5 seconds
- **Sample Rate**: 48000 Hz
- **Bit Depth**: 32-bit float

### **2. Sine Sweep (20Hz - 20kHz)**
- **Purpose**: Frequency response across spectrum
- **Duration**: 10 seconds
- **Sample Rate**: 48000 Hz

### **3. White Noise (-12dB)**
- **Purpose**: Statistical behavior, entropy handling
- **Duration**: 5 seconds
- **Sample Rate**: 48000 Hz

### **4. Kick Drum (Dry)**
- **Purpose**: Transient response, punch parameter
- **Duration**: 2 seconds (single hit + decay)
- **Sample Rate**: 48000 Hz

### **5. Commercial Mix**
- **Purpose**: Real-world scenario
- **Duration**: 10 seconds
- **Sample Rate**: 48000 Hz

---

## 🎛️ TEST PRESETS

### **Preset 1: Default**
```xml
<STATE>
  <drive_master>50.0</drive_master>
  <math_mode>0</math_mode>  <!-- Golden Ratio -->
  <mix>100.0</mix>
  <!-- All other params at default -->
</STATE>
```

### **Preset 2: Extreme Drive**
```xml
<STATE>
  <drive_master>100.0</drive_master>
  <math_mode>0</math_mode>
  <dynamics>75.0</dynamics>
  <punch>50.0</punch>
</STATE>
```

### **Preset 3: Network Active**
```xml
<STATE>
  <group_id>1</group_id>
  <role>0</role>  <!-- Listener -->
  <net_sens>50.0</net_sens>
  <net_depth>75.0</net_depth>
</STATE>
```

### **Preset 4: Full Mojo**
```xml
<STATE>
  <heat>100.0</heat>
  <analog_drift>75.0</analog_drift>
  <variance>50.0</variance>
  <entropy>50.0</entropy>
  <noise>25.0</noise>
</STATE>
```

---

## ✅ ACCEPTANCE CRITERIA

### **Per-Sample Difference Test**
```cpp
bool audioIdentical(const AudioBuffer<float>& reference, 
                    const AudioBuffer<float>& test) {
    if (reference.getNumSamples() != test.getNumSamples()) 
        return false;
    
    float maxDiff = 0.0f;
    for (int ch = 0; ch < 2; ++ch) {
        for (int i = 0; i < reference.getNumSamples(); ++i) {
            float diff = std::abs(reference.getSample(ch, i) - test.getSample(ch, i));
            maxDiff = std::max(maxDiff, diff);
        }
    }
    
    // Threshold: float32 precision (~-120dB)
    const float threshold = 1e-6f;
    return maxDiff < threshold;
}
```

### **Success Criteria**:
- ✅ All 5 signals × 4 presets = **20 tests pass**
- ✅ Max per-sample difference < **1e-6** (0.0001%)
- ✅ Zero crashes
- ✅ Zero NaN/Inf values in output

---

## 🚀 HOW TO RUN

### **Step 1: Generate Reference Audio (v1.30)**
```bash
# Build current version
cmake --build build --target Cohera_Saturator_Standalone

# Run reference generator
./build/tests/generate_reference_audio

# Output: reference_audio/*.wav files
```

### **Step 2: Run Regression Tests (After Refactoring)**
```bash
# Build refactored version
cmake --build build --target Cohera_Saturator_Standalone

# Run tests
./build/tests/test_audio_regression

# Output:
# ✅ All tests passed (20/20)
# or
# ❌ Test failed: sine_440hz_extreme_drive.wav
#    Max difference: 3.2e-5 (threshold: 1e-6)
```

---

## 📊 CONTINUOUS INTEGRATION

Add to CI pipeline:
```yaml
# .github/workflows/regression.yml
name: Audio Regression Tests
on: [push, pull_request]

jobs:
  test:
    runs-on: macos-latest
    steps:
      - uses: actions/checkout@v2
      - name: Build
        run: cmake --build build
      - name: Run Regression Tests
        run: ./build/tests/test_audio_regression
      - name: Upload Diff (if failed)
        if: failure()
        uses: actions/upload-artifact@v2
        with:
          name: audio-diff
          path: tests/regression/test_audio/*.wav
```

---

## 🛠️ TROUBLESHOOTING

### **Test Fails with Small Difference (e.g., 1e-5)**
**Possible Causes**:
1. Float precision accumulation (acceptable if < 1e-5)
2. Different compiler optimization flags
3. Different CPU (SSE vs AVX)

**Solution**: Relax threshold to 1e-5 if difference is consistent and inaudible.

### **Test Fails with Large Difference (e.g., 0.1)**
**Possible Causes**:
1. Algorithm changed (BUG!)
2. Parameter order changed
3. DSP chain order changed

**Solution**: Investigate code diff, revert changes.

---

**Status**: 🟡 Setup Complete, Awaiting Reference Audio Generation
