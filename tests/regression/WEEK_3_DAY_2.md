# Week 3 Day 2: 60 FPS Visualizer + Performance Profiling
**Date**: 2025-11-23  
**Focus**: Frame Rate Upgrade & Metrics  
**Status**: ✅ COMPLETE

## Objective
Increase visualizer refresh rate from 30 FPS to 60 FPS for smoother animation, and add performance profiling to measure actual paint times.

## Changes Implemented

### 1. Frame Rate Upgrade: 30 → 60 FPS
**File**: `src/ui/Components/SpectrumVisor.h`

**Change**:
```cpp
// OLD
startTimerHz(30);  // 30 FPS

// NEW
startTimerHz(60);  // 60 FPS - плавная анимация
```

**Impact**:
- ✅ Doubled refresh rate for smoother spectrum animation
- ✅ Frame time budget: 16.67ms (was 33.33ms)
- ✅ Possible thanks to Day 1 optimizations (freed up CPU headroom)

### 2. Performance Profiling (Debug Only)
**File**: `src/ui/Components/SpectrumVisor.h`

**Added**:
```cpp
// In constructor
#if JUCE_DEBUG
enableProfiling = true;
#endif

// In paint()
#if JUCE_DEBUG
auto startTime = juce::Time::getMillisecondCounterHiRes();
// ... paint code ...
auto paintTime = endTime - startTime;
frameCount++;
avgPaintTime = (avgPaintTime * (frameCount - 1) + paintTime) / frameCount;
maxPaintTime = juce::jmax(maxPaintTime, paintTime);
#endif

// In destructor - print stats
DBG("SpectrumVisor Performance Stats:");
DBG("  Frames: " << frameCount);
DBG("  Avg Paint Time: " << avgPaintTime << " ms");
DBG("  Max Paint Time: " << maxPaintTime << " ms");
```

**Features**:
- ✅ Zero overhead in Release builds (all profiling code is `#if JUCE_DEBUG`)
- ✅ Tracks average and maximum paint times
- ✅ Prints statistics on component destruction
- ✅ Uses high-resolution timer for accurate measurements

### 3. Member Variables
```cpp
#if JUCE_DEBUG
bool enableProfiling = false;
int frameCount = 0;
double avgPaintTime = 0.0;
double maxPaintTime = 0.0;
#endif
```

## Performance Analysis

### Frame Time Budget
| FPS | Frame Time | Paint Budget | Headroom |
|-----|------------|--------------|----------|
| 30  | 33.33 ms   | ~25 ms       | Good     |
| **60** | **16.67 ms** | **~12 ms** | **Tight** |

### Expected Paint Times (Post-Optimization)
Based on Day 1 optimizations:
- **Before**: ~1.2 ms (with pow() calls)
- **After**: ~0.8 ms (with lookup table)
- **60 FPS Budget**: 16.67 ms
- **Safety Margin**: ~15.87 ms (plenty of headroom!)

### Why 60 FPS is Safe
1. **FFT process time**: ~0.8 ms (optimized)
2. **Paint time**: ~0.5-1.0 ms (gradient fills + paths)
3. **Total**: ~1.3-1.8 ms per frame
4. **Budget**: 16.67 ms
5. **Headroom**: **~15 ms** (90% free!)

## Profiling Usage

### How to Use
1. **Build in Debug mode**: `cmake --build build --config Debug`
2. **Run the plugin**: Open in DAW or standalone
3. **Open the visualizer**: Let it run for a few seconds
4. **Close the plugin**: Check console output for stats

### Example Output
```
SpectrumVisor Performance Stats:
  Frames: 3600
  Avg Paint Time: 0.847 ms
  Max Paint Time: 1.234 ms
```

### Interpretation
- **Avg < 10 ms**: Excellent (60 FPS sustainable)
- **Avg < 16 ms**: Good (60 FPS with headroom)
- **Avg > 16 ms**: Problematic (frame drops likely)
- **Max spikes**: Occasional spikes are OK, sustained high values are not

## Regression Testing
All audio regression tests pass with **0.0 dB MaxDiff**:
```
✅ Kick Default        MaxDiff: 0 (-100 dB)
✅ Kick Extreme        MaxDiff: 0 (-100 dB)
✅ Kick Mojo           MaxDiff: 0 (-100 dB)
✅ Snare Default       MaxDiff: 0 (-100 dB)
✅ Snare Network       MaxDiff: 0 (-100 dB)
✅ Bass Default        MaxDiff: 0 (-100 dB)
✅ Guitar Mojo         MaxDiff: 0 (-100 dB)

Summary: 7/7 tests passed.
```

## Technical Notes

### Why High-Resolution Timer?
`juce::Time::getMillisecondCounterHiRes()` provides microsecond precision:
- Standard `getMillisecondCounter()`: ~1 ms resolution
- `getMillisecondCounterHiRes()`: ~0.001 ms resolution
- Essential for measuring sub-millisecond paint times

### Debug-Only Profiling
Using `#if JUCE_DEBUG` ensures:
- **Zero overhead** in Release builds
- **No performance impact** on end users
- **Useful metrics** during development

### Rolling Average Calculation
```cpp
avgPaintTime = (avgPaintTime * (frameCount - 1) + paintTime) / frameCount;
```
This is an **exponential moving average** that:
- Weights all frames equally
- Converges to true average over time
- Requires no storage of historical data

## Visual Improvements

### Before (30 FPS)
- Spectrum updates every 33.33 ms
- Noticeable "stepping" in fast transients
- Acceptable for static content

### After (60 FPS)
- Spectrum updates every 16.67 ms
- Smoother animation, especially on transients
- Feels more "alive" and responsive
- Better matches modern display refresh rates (60 Hz)

## Next Steps (Week 3 Day 3)
1. **Optimize gradient fills** - Consider simpler rendering for even better performance
2. **Add dirty region tracking** - Only repaint changed areas
3. **Implement VBlankAttachment** - Sync to display refresh for tear-free rendering
4. **Profile other UI components** - Apply same optimization strategy to knobs, meters, etc.

## Verification Commands
```bash
# Build
cmake --build build --target Cohera_Saturator

# Test
./build/tests/test_audio_regression

# Profile (Debug build)
cmake --build build --config Debug
# Run plugin, check console output on close
```

---
**Signed-off**: Week 3 Day 2 Complete - 60 FPS visualizer with profiling ✅
