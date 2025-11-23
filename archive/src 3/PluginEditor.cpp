#include "PluginEditor.h"
#include "PluginProcessor.h"

CoheraSaturatorAudioProcessorEditor::CoheraSaturatorAudioProcessorEditor(CoheraSaturatorAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p)
{
    // Базовый размер окна
    setSize(600, 400);
}

CoheraSaturatorAudioProcessorEditor::~CoheraSaturatorAudioProcessorEditor()
{
}

void CoheraSaturatorAudioProcessorEditor::paint(juce::Graphics& g)
{
    // Темный, "дорогой" фон (Cohera Style)
    g.fillAll(juce::Colour::fromRGB(20, 20, 25));

    g.setColour(juce::Colours::white);
    g.setFont(24.0f);
    
    // Центрированный текст
    g.drawFittedText("Cohera Saturator", getLocalBounds(), juce::Justification::centred, 1);
    
    g.setFont(14.0f);
    g.setColour(juce::Colours::grey);
    g.drawFittedText("Phase 1: Skeleton Init", getLocalBounds().translated(0, 30), juce::Justification::centred, 1);
}

void CoheraSaturatorAudioProcessorEditor::resized()
{
    // Здесь будет лейаут компонентов
}
