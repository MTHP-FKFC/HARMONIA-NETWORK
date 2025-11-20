#pragma once
#include "JuceHeader.h"
#include "PluginProcessor.h"
#include "ui/CoheraLookAndFeel.h"
#include "ui/components/SaturationCore.h"
#include "ui/components/NetworkBrain.h"

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
    
    // Панели
    SaturationCore saturationCore;
    NetworkBrain networkBrain;
    
    // Footer controls
    juce::Slider mixSlider;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> mixAtt;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CoheraSaturatorAudioProcessorEditor)
};