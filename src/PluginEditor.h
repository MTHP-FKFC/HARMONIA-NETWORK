#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_graphics/juce_graphics.h>
#include "PluginProcessor.h"

class CoheraSaturatorAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    CoheraSaturatorAudioProcessorEditor(CoheraSaturatorAudioProcessor&);

    ~CoheraSaturatorAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    // Ссылка на процессор для доступа к параметрам
    CoheraSaturatorAudioProcessor& audioProcessor;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(CoheraSaturatorAudioProcessorEditor)
};
