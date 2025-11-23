#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <mutex>
#include <vector>
#include <array>

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
  Cohera::ParameterManager& getParamManager() { return paramManager; }
  
  // Core engine access (for ReactorKnob and advanced UI)
  Cohera::ProcessingEngine& getProcessingEngine() { return processingEngine; }
  
  // Metrics (thread-safe atomics)
  float getInputRMS() const { return processingEngine.getInputRMS(); }
  float getOutputRMS() const { return outputRMS.load(); }
  float getTransientLevel() const { return lastTransientLevel.load(); }
  
  // Thermal state for BioScanner visualization
  float getCurrentTemperature() const { return currentThermalState.load(); }
  float getNormalizedTemperature() const { 
    return juce::jmap(currentThermalState.load(), 20.0f, 120.0f, 0.0f, 1.0f); 
  }
  
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
  
  // Transfer function visualization (for NebulaShaper) - LOCK-FREE
  void pushVisualizerData(float input, float output) {
    juce::AbstractFifo::ScopedWrite writer(visualizerFifo, 1);
    if (writer.blockSize1 > 0) {
      visualizerBuffer[writer.startIndex1] = {input, output};
    }
    if (writer.blockSize2 > 0) {
      visualizerBuffer[writer.startIndex2] = {input, output};
    }
  }
  
  bool popVisualizerData(float& input, float& output) {
    juce::AbstractFifo::ScopedRead reader(visualizerFifo, 1);
    if (reader.blockSize1 > 0) {
      auto point = visualizerBuffer[reader.startIndex1];
      input = point.first;
      output = point.second;
      return true;
    }
    return false;
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
  std::atomic<float> currentThermalState{20.0f}; // BioScanner temperature
  
  // Transfer function visualizer (NebulaShaper) - LOCK-FREE
  juce::AbstractFifo visualizerFifo { 4096 };
  std::array<std::pair<float, float>, 4096> visualizerBuffer;
  
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