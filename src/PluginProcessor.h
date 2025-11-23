#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <mutex>
#include <vector>

// Core architecture
#include "CoheraTypes.h"
#include "engine/ProcessingEngine.h"
#include "network/NetworkManager.h"
#include "parameters/ParameterManager.h"
#include "parameters/ParameterSet.h"
#include "ui/SimpleFFT.h"

/**
 * @brief Main Audio Processor for Cohera Saturator
 * 
 * Clean Architecture v1.30:
 * - This class is a thin JUCE wrapper (Presentation Layer)
 * - All DSP logic lives in ProcessingEngine (Business Logic Layer)
 * - Parameter management is delegated to ParameterManager
 * 
 * Responsibilities:
 * 1. JUCE lifecycle management (prepareToPlay, processBlock, etc.)
 * 2. Parameter state (AudioProcessorValueTreeState)
 * 3. UI data access (RMS, FFT, visualizer data)
 * 4. Network instance registration
 */
class CoheraSaturatorAudioProcessor : public juce::AudioProcessor {
public:
  CoheraSaturatorAudioProcessor();
  ~CoheraSaturatorAudioProcessor() override;

  //==========================================================================
  // JUCE LIFECYCLE
  //==========================================================================
  
  void prepareToPlay(double sampleRate, int samplesPerBlock) override;
  void releaseResources() override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

  //==========================================================================
  // EDITOR
  //==========================================================================
  
  juce::AudioProcessorEditor* createEditor() override;
  bool hasEditor() const override { return true; }

  //==========================================================================
  // STATE PERSISTENCE
  //==========================================================================
  
  void getStateInformation(juce::MemoryBlock& destData) override;
  void setStateInformation(const void* data, int sizeInBytes) override;

  //==========================================================================
  // PLUGIN INFO
  //==========================================================================
  
  const juce::String getName() const override { return "Cohera Saturator"; }
  bool acceptsMidi() const override { return false; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  double getTailLengthSeconds() const override { return 0.0; }

  //==========================================================================
  // PROGRAMS (Not used, but required by JUCE)
  //==========================================================================
  
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return {}; }
  void changeProgramName(int, const juce::String&) override {}

  //==========================================================================
  // PUBLIC API FOR UI
  //==========================================================================
  
  // Parameter access
  juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
  
  // Core engine access (for ReactorKnob and advanced UI)
  Cohera::ProcessingEngine& getProcessingEngine() { return processingEngine; }
  
  // Metrics (thread-safe atomics)
  float getInputRMS() const { return processingEngine.getInputRMS(); }
  float getOutputRMS() const { return outputRMS.load(); }
  float getTransientLevel() const { return lastTransientLevel.load(); }
  
  // Network activity (for SmartReactorKnob)
  float getNetworkInput() const { return processingEngine.getInputRMS(); }
  float getModIntensity() const { return lastTransientLevel.load(); }
  
  // Spectrum analyzer
  SimpleFFT& getAnalyzer() { return analyzer; }
  const std::array<float, 512>& getFFTData() const { return analyzer.getScopeData(); }
  bool isFFTActive() const { return analyzer.isDataReady(); }
  void processFFTForGUI() { analyzer.process(); }
  
  // Gain reduction meters (6 bands)
  const std::array<float, 6>& getGainReduction() const {
    return processingEngine.getGainReductionValues();
  }
  
  // Transfer function visualization (for NebulaShaper)
  void pushVisualizerData(float input, float output) {
    std::lock_guard<std::mutex> lock(visualizerMutex);
    visualizerFIFO.push_back({input, output});
    if (visualizerFIFO.size() > 2000) {
      visualizerFIFO.erase(visualizerFIFO.begin());
    }
  }
  
  bool popVisualizerData(float& input, float& output) {
    std::lock_guard<std::mutex> lock(visualizerMutex);
    if (visualizerFIFO.empty())
      return false;
    auto point = visualizerFIFO.front();
    visualizerFIFO.erase(visualizerFIFO.begin());
    input = point.first;
    output = point.second;
    return true;
  }

private:
  //==========================================================================
  // PARAMETER SYSTEM
  //==========================================================================
  
  juce::AudioProcessorValueTreeState apvts;
  Cohera::ParameterManager paramManager;
  
  juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

  //==========================================================================
  // DSP CORE (Clean Architecture)
  //==========================================================================
  
  Cohera::ProcessingEngine processingEngine;

  //==========================================================================
  // VISUALIZATION & ANALYSIS
  //==========================================================================
  
  SimpleFFT analyzer;
  
  // Atomics for UI thread-safe access
  std::atomic<float> outputRMS{0.0f};
  std::atomic<float> lastTransientLevel{0.0f};
  
  // Transfer function visualizer (NebulaShaper)
  std::vector<std::pair<float, float>> visualizerFIFO;
  std::mutex visualizerMutex;
  
  // Pre-allocated buffers (CRITICAL: No heap allocations in processBlock!)
  juce::AudioBuffer<float> dryBuffer;
  juce::AudioBuffer<float> monoBuffer;

  //==========================================================================
  // NETWORK
  //==========================================================================
  
  int myInstanceIndex = -1; // Slot ID in NetworkManager

  //==========================================================================
  // THREAD SAFETY
  //==========================================================================
  
  juce::CriticalSection processLock; // Prevents race conditions during state load

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CoheraSaturatorAudioProcessor)
};