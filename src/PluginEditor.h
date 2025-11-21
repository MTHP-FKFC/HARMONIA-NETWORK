#pragma once

#include "PluginProcessor.h"
#include "ui/CoheraLookAndFeel.h"
#include "ui/Components/EnergyLink.h"
// #include "ui/Components/NetworkBrain.h"
#include "ui/components/SmartReactorKnob.h"
#include "ui/visuals/NebulaShaper.h"
#include "ui/ControlGroup.h"
#include "ui/SpectrumVisor.h"
#include "ui/components/ReactorKnob.h"
#include "ui/visuals/CosmicDust.h"
#include "ui/visuals/GlitchOverlay.h"
#include "ui/visuals/HeadsUpDisplay.h"
#include "ui/visuals/HorizonGrid.h"
#include "ui/visuals/NeuralLink.h"
#include "ui/visuals/TechDecor.h"
#include "ui/visuals/TextureOverlay.h"
#include "ui/visuals/TransferFunctionDisplay.h"
#include "ui/visuals/ScreenShaker.h"
#include "ui/visuals/BioScanner.h"
#include <juce_gui_basics/juce_gui_basics.h>
// // #include "ui/Components/InteractionMeter.h"

class CoheraSaturatorAudioProcessorEditor : public juce::AudioProcessorEditor,
                                            private juce::Timer {
public:
  CoheraSaturatorAudioProcessorEditor(CoheraSaturatorAudioProcessor &);
  ~CoheraSaturatorAudioProcessorEditor() override;

  void paint(juce::Graphics &) override;
  void paintOverChildren(juce::Graphics &) override;
  void resized() override;
  void timerCallback() override;

private:
  void setupKnob(juce::Slider &s, juce::String paramId,
                 juce::String displayName, juce::Colour c);
  void setupReactorKnob(CoheraUI::SmartReactorKnob& s, juce::String paramId,
                        juce::String displayName);

  // Layout helpers
  void layoutSaturation(juce::Rectangle<int> area);
  void layoutNetwork(juce::Rectangle<int> area);
  void layoutFooter(juce::Rectangle<int> area);

  CoheraSaturatorAudioProcessor &audioProcessor;

  static inline std::unique_ptr<CoheraUI::CoheraLookAndFeel> sharedLookAndFeel;

  // Components
  SpectrumVisor spectrumVisor;
  EnergyLink energyLink; // Центральный поток энергии
  NebulaShaper nebulaShaper { audioProcessor }; // Nebula visualizer
  // NetworkBrain networkBrain; // Network Intelligence Panel - temporarily disabled
  // InteractionMeter interactionMeter; // Network activity meter - temporarily disabled

  // Groups
  ControlGroup satGroup{"SATURATION CORE", CoheraUI::kOrangeNeon};
  ControlGroup netGroup{"NETWORK INTELLIGENCE", CoheraUI::kCyanNeon};

  // Controls
  juce::ComboBox groupSelector, roleSelector, mathModeSelector, netModeSelector,
      netSatSelector, qualitySelector;
  juce::TextButton cascadeButton;

  // Saturation Knobs
  ReactorKnob driveSlider;
  juce::Slider tightenSlider, smoothSlider, punchSlider, dynamicsSlider;

  // Network Smart Knobs (Strategy Pattern)
  std::unique_ptr<CoheraUI::SmartReactorKnob> netSensKnob;
  std::unique_ptr<CoheraUI::SmartReactorKnob> netDepthKnob;

  // Global / Mojo Knobs (Footer)
  juce::Slider mixSlider, outputSlider, focusSlider;
  juce::Slider heatSlider, driftSlider, varianceSlider, entropySlider,
      noiseSlider; // Mojo ручки

  // Buttons & Selectors
  juce::TextButton deltaButton;
  juce::TextButton viewSwitchButton;
  bool showNebula = false; // View switch state

  // Attachments
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      groupAttachment, roleAttachment, mathModeAttachment,
      qualityAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      driveAttachment, dynamicsAttachment, outputAttachment, focusAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      heatAttachment, driftAttachment, varianceAttachment, entropyAttachment,
      noiseAttachment;
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      netSensAttachment, netDepthAttachment;
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
  TransferFunctionDisplay shaperScope;
    TechDecor techDecor;
    BioScanner bioScanner;
    GlitchOverlay glitchOverlay;

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(
      CoheraSaturatorAudioProcessorEditor)
};