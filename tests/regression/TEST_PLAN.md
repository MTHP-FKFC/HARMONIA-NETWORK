# Cohera Saturator - Audio Quality & Regression Test Plan 🛡️

This document outlines the comprehensive test suite for verifying audio quality, signal integrity, and regression stability of the Cohera Saturator plugin.

## 0. General Rules & Preconditions 📏

To ensure reproducible and deterministic results, all regression tests must adhere to the following rules:

1.  **Deterministic Mode**:
    For all tests requiring strict null comparison, the following parameters must be set to **0** or **OFF** to eliminate randomness:
    *   `analog_drift = 0`
    *   `noise = 0`
    *   `entropy = 0`
    *   `variance = 0`
    *   Any other random modulation sources.

2.  **Sample Rates**:
    Tests should ideally be run at multiple sample rates:
    *   **44.1 kHz** (Standard)
    *   **48 kHz** (Video Standard)
    *   **96 kHz** (High Res)

3.  **Buffer Sizes**:
    Audio output must be invariant across different buffer sizes:
    *   **64 samples** (Low latency)
    *   **256 samples** (Standard)
    *   **1024 samples** (High latency/Offline)

4.  **Null Criteria**:
    *   **Strict Null**: Difference peak ≤ **-120 dBFS** (Bit-transparent).
    *   **Tolerance Null**: Difference peak ≤ **-100 dBFS** (Acceptable for DSP with float errors/oversampling).

---

## 1. Basic Null Cases (Sanity Checks) ✅

These tests verify the fundamental integrity of the signal path.

### Test 1: Perfectly Transparent (Neutral Preset)
**Goal**: Verify bit-transparency at neutral settings.
*   **Input**: Stereo Loop (Music/Drums), -6 to -12 dBFS.
*   **Settings**:
    *   `drive_master = 0`
    *   `mix = 0%`
    *   `output_gain = 0 dB`
    *   All tone/dynamics/network parameters at default/neutral.
    *   Random parameters = 0.
*   **Pass Criteria**: Output `y` - Input `x` ≤ **-120 dBFS**.

### Test 2: Dry-Only Integrity
**Goal**: Verify that `mix = 0%` passes the dry signal perfectly, regardless of internal processing state.
*   **Input**: Stereo Loop.
*   **Settings**:
    *   `drive_master = 50-70%` (Significant processing active internally)
    *   `mix = 0%`
    *   `output_gain = 0 dB`
*   **Pass Criteria**: Output `y` - Input `x` ≤ **-120 dBFS**.

### Test 3: Wet-Only & Mix Law
**Goal**: Verify the mix formula: `out = dry + (wet - dry) * mix`.
*   **Input**: Mono Sine (-12 dBFS) or simple transient.
*   **Procedure**:
    1.  Capture `dry` (mix = 0%).
    2.  Capture `wet` (mix = 100%).
    3.  Render `plugin_out` at `mix = 50%`.
    4.  Calculate `theoretical = dry + (wet - dry) * 0.5`.
*   **Pass Criteria**: `plugin_out` - `theoretical` ≤ **-100 dBFS**.

### Test 4: Output Gain Linearity
**Goal**: Verify `output_gain` is a simple linear post-gain.
*   **Input**: Stereo Loop.
*   **Settings**: Heavy saturation active.
*   **Procedure**:
    1.  Render `y0` at `output_gain = 0 dB`.
    2.  Render `y6` at `output_gain = +6 dB`.
    3.  Calculate `scaled = y0 * 2.0` (+6 dB is roughly 2x amplitude).
*   **Pass Criteria**: `y6` - `scaled` ≤ **-100 dBFS**.

### Test 5: Bypass vs Neutral
**Goal**: Verify DAW bypass is equivalent to the neutral preset.
*   **Method**: Manual DAW Test.
*   **Procedure**:
    1.  Track A: Dry signal + Plugin (Neutral Preset).
    2.  Track B: Dry signal (No Plugin), Phase Inverted.
    3.  Sum A + B.
*   **Pass Criteria**: Sum silence ≤ **-100 dBFS**.

---

## 2. Invariance Tests 🔄

### Test 6: Determinism (Run-to-Run)
**Goal**: Verify that two consecutive runs with identical settings produce identical output.
*   **Input**: Any Loop.
*   **Settings**: Heavy processing, Random params = 0.
*   **Procedure**: Render `y1` then `y2`.
*   **Pass Criteria**: `y1` - `y2` = **0** (Absolute silence or -150 dBFS).

### Test 7: Block Size Invariance
**Goal**: Verify output independence from buffer size.
*   **Input**: Any Loop.
*   **Settings**: Deterministic mode.
*   **Procedure**:
    1.  Render `y64` with buffer size 64.
    2.  Render `y1024` with buffer size 1024.
*   **Pass Criteria**: `y64` - `y1024` ≤ **-120 dBFS**.

### Test 8: Sample Rate Consistency
**Goal**: Verify consistent behavior across sample rates.
*   **Procedure**:
    1.  Upsample source `x` to 44.1k, 48k, 96k.
    2.  Process each to get `y44`, `y48`, `y96`.
    3.  Downsample `y48` and `y96` back to 44.1k.
    4.  Compare spectral content / dynamics.
*   **Pass Criteria**: General character matches; harmonic content within ~1-2 dB tolerance.

---

## 3. Feature-Specific Tests 🎧

### Test 9: Delta Listen Accuracy
**Goal**: Verify `Delta = Wet - Dry`.
*   **Input**: Short transient or sine.
*   **Settings**: Deterministic.
*   **Procedure**:
    1.  Render `y_wet` (Normal mode).
    2.  Render `y_delta` (Delta Listen ON).
    3.  Calculate `theoretical_delta = y_wet - x_dry`.
*   **Pass Criteria**: `y_delta` - `theoretical_delta` ≤ **-100 dBFS**.

---

## 4. Nonlinear & Quality Tests 📊

### Test 10: THD (Total Harmonic Distortion)
**Goal**: Characterize saturation onset and harmonic profile.
*   **Input**: 1 kHz Sine at -30, -18, -12, -6, -3 dBFS.
*   **Analysis**: FFT measurement of H2, H3, H4, etc.
*   **Expectation**: Smooth rise in harmonics with input level; no sudden jumps.

### Test 11: IMD (Intermodulation Distortion)
**Goal**: Check for clean saturation without excessive non-harmonic dirt.
*   **Input**: Two tones (e.g., 500 Hz & 700 Hz) at -18 dBFS.
*   **Analysis**: Check sum/difference tones.

### Test 12: Aliasing Check
**Goal**: Verify anti-aliasing filter performance.
*   **Input**: Sine sweep (20 Hz - 30 kHz) or high-frequency tones.
*   **Settings**: Aggressive Drive, Oversampling ON vs OFF.
*   **Analysis**: Spectrogram check for foldback frequencies.
*   **Expectation**: Visible reduction in aliasing with Oversampling ON.

---

## 5. Implementation Strategy 🛠️

1.  **Test Assets**: Store input WAVs in `tests/audio_inputs/`.
2.  **Reference Files**: Store validated outputs in `tests/reference/1.0.0/`.
3.  **Automation**: Use `TestAudioRegression` tool with a config file (JSON/YAML) to define test cases and pass/fail thresholds.
4.  **CI Integration**: Run the full suite on every commit/PR.
