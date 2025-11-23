#pragma once

#include "PluginProcessor.h"
#include "ui/CoheraLookAndFeel.h"
#include "ui/Components/NetworkBrain.h"
#include "ui/Components/ReactorKnob.h"
#include "ui/ControlGroup.h"
#include "ui/SpectrumVisor.h"
#include "ui/visuals/BioScanner.h"
#include "ui/visuals/CosmicDust.h"
#include "ui/visuals/GlitchOverlay.h"
#include "ui/visuals/HeadsUpDisplay.h"
#include "ui/visuals/HorizonGrid.h"
#include "ui/visuals/NebulaShaper.h"
#include "ui/visuals/NeuralLink.h"
#include "ui/visuals/PlasmaCore.h"
#include "ui/visuals/ScreenShaker.h"
#include "ui/visuals/TechDecor.h"
#include "ui/visuals/TextureOverlay.h"
#include <functional>
#include <memory>
#include <juce_gui_basics/juce_gui_basics.h>
// #include "ui/Components/InteractionMeter.h"

class CoheraSaturatorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                            private juce::Timer {
public:
  CoheraSaturatorAudioProcessorEditor(CoheraSaturatorAudioProcessor &);
  ~CoheraSaturatorAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void paintOverChildren(juce::Graphics &) override;
  void resized() override;
  void visibilityChanged() override;
  void timerCallback() override;

private:
  void setupKnob(juce::Slider &s, juce::String paramId,
                 juce::String displayName, juce::Colour c);

  // Layout helpers
  void layoutSaturation(juce::Rectangle<int> area);
  void layoutNetwork(juce::Rectangle<int> area);
  void layoutFooter(juce::Rectangle<int> area);

  CoheraSaturatorAudioProcessor &audioProcessor;

  static inline std::unique_ptr<CoheraUI::CoheraLookAndFeel> sharedLookAndFeel;

  // Components
  SpectrumVisor spectrumVisor;
  NetworkBrain networkBrain; // Network Intelligence Panel

  // Groups
  ControlGroup satGroup{"SATURATION CORE", CoheraUI::kOrangeNeon};
  ControlGroup netGroup{"NETWORK INTELLIGENCE", CoheraUI::kCyanNeon};

  // Controls
  juce::ComboBox groupSelector, roleSelector, mathModeSelector, netModeSelector,
      netSatSelector,
      qualitySelector; // , scaleSelector; // TODO: Scaling disabled
  juce::TextButton cascadeButton;

  // float currentScale = 1.0f; // TODO: Scaling disabled

  // Saturation Knobs
  ReactorKnob driveSlider;
  juce::Slider tightenSlider, smoothSlider, punchSlider, dynamicsSlider;

  // Global / Mojo Knobs (Footer)
  juce::Slider mixSlider, outputSlider, focusSlider;
  juce::Slider heatSlider, driftSlider, varianceSlider, entropySlider,
      noiseSlider; // Mojo ручки

  // Buttons & Selectors
  juce::TextButton deltaButton;

  // Attachments
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      groupAttachment, roleAttachment, mathModeAttachment, qualityAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      driveAttachment, dynamicsAttachment, outputAttachment, focusAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      heatAttachment, driftAttachment, varianceAttachment, entropyAttachment,
      noiseAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      deltaAttachment, cascadeAttachment;
  std::vector<
      std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>>
      sliderAttachments;

  std::unique_ptr<CoheraUI::CoheraLookAndFeel> lookAndFeel;

  // New Visual System v2.0
  ScreenShaker screenShaker;
  juce::Component shakerContainer;
  CosmicDust cosmicDust;
  HorizonGrid horizonGrid;
  HeadsUpDisplay hud;
  TextureOverlay textureOverlay;
  NeuralLink neuralLink;
  std::unique_ptr<NebulaShaper> nebulaShaper; // Cosmic particle visualizer
  TechDecor techDecor;
  std::unique_ptr<BioScanner> bioScanner; // Thermal core visualizer
  GlitchOverlay glitchOverlay;
  PlasmaCore plasmaCore; // Central plasma energy core

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
      CoheraSaturatorAudioProcessorEditor)
};