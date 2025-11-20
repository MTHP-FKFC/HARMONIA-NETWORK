#pragma once
#include "JuceHeader.h"
#include "PluginProcessor.h"
#include "ui/CoheraLookAndFeel.h"
#include "ui/components/TopBar.h"
#include "ui/components/SaturationCore.h"
#include "ui/components/NetworkBrain.h"
#include "ui/components/SpectrumVisor.h"

class CoheraSaturatorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    CoheraSaturatorAudioProcessorEditor(CoheraSaturatorAudioProcessor& p);

    ~CoheraSaturatorAudioProcessorEditor() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    CoheraSaturatorAudioProcessor& audioProcessor;
    CoheraLookAndFeel lnf;

    TopBar topBar;
    SpectrumVisor spectrumVisor;
    SaturationCore saturationCore;
    NetworkBrain networkBrain;
    
    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CoheraSaturatorAudioProcessorEditor)
};