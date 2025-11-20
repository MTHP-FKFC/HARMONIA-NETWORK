#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "ui/CoheraLookAndFeel.h"
#include "ui/SpectrumVisor.h"
#include "ui/ControlGroup.h"
#include "ui/Components/EnergyLink.h"
#include "ui/components/ReactorKnob.h"
// #include "ui/Components/InteractionMeter.h"

class CoheraSaturatorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    CoheraSaturatorAudioProcessorEditor (CoheraSaturatorAudioProcessor&);
    ~CoheraSaturatorAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void setupKnob(juce::Slider& s, juce::String paramId, juce::String displayName, juce::Colour c);

    // Layout helpers
    void layoutSaturation(juce::Rectangle<int> area);
    void layoutNetwork(juce::Rectangle<int> area);
    void layoutFooter(juce::Rectangle<int> area);

    CoheraSaturatorAudioProcessor& audioProcessor;

    static inline std::unique_ptr<CoheraUI::CoheraLookAndFeel> sharedLookAndFeel;

    // Components
    SpectrumVisor spectrumVisor;
    EnergyLink energyLink; // Центральный поток энергии
    // InteractionMeter interactionMeter; // Наш новый метр - временно отключен

    // Groups
    ControlGroup satGroup { "SATURATION CORE", CoheraUI::kOrangeNeon };
    ControlGroup netGroup { "NETWORK INTELLIGENCE", CoheraUI::kCyanNeon };

    // Controls
    juce::ComboBox groupSelector, roleSelector, mathModeSelector, netModeSelector, netSatSelector, qualitySelector;
    juce::TextButton cascadeButton;

    // Saturation Knobs
    ReactorKnob driveSlider;
    juce::Slider tightenSlider, smoothSlider, punchSlider, dynamicsSlider;

    // Network Knobs
    juce::Slider netSensSlider, netDepthSlider, netSmoothSlider;

    // Global / Mojo Knobs (Footer)
    juce::Slider mixSlider, outputSlider, focusSlider;
    juce::Slider heatSlider, driftSlider, varianceSlider, entropySlider, noiseSlider; // Mojo ручки

    // Buttons & Selectors
    juce::TextButton deltaButton;

    // Attachments
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> groupAttachment, roleAttachment, mathModeAttachment, netModeAttachment, netSatAttachment, qualityAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAttachment, dynamicsAttachment, outputAttachment, focusAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> heatAttachment, driftAttachment, varianceAttachment, entropyAttachment, noiseAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> deltaAttachment, cascadeAttachment;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;

    std::unique_ptr<CoheraUI::CoheraLookAndFeel> lookAndFeel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CoheraSaturatorAudioProcessorEditor)
};