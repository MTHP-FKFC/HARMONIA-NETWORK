#include "PluginEditor.h"

CoheraSaturatorAudioProcessorEditor::CoheraSaturatorAudioProcessorEditor(CoheraSaturatorAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      saturationCore(p.getAPVTS()),
      networkBrain(p.getAPVTS())
{
    // Применяем стиль
    setLookAndFeel(&lnf);
    
    // Добавляем компоненты
    addAndMakeVisible(saturationCore);
    addAndMakeVisible(networkBrain);
    
    // Mix & Output (Глобальные, внизу)
    mixSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    mixSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 50, 20);
    addAndMakeVisible(mixSlider);
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "mix", mixSlider);
    
    // Размер окна
    setSize(800, 600);
    setResizable(true, true);
}

CoheraSaturatorAudioProcessorEditor::~CoheraSaturatorAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void CoheraSaturatorAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
    
    // Заголовок (Заглушка для TopBar)
    g.setColour(juce::Colours::white);
    g.setFont(20.0f);
    g.drawText("COHERA SATURATOR", getLocalBounds().removeFromTop(40), juce::Justification::centred);
}

void CoheraSaturatorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    
    // Header
    auto header = area.removeFromTop(40);
    
    // Footer (Mix knob)
    auto footer = area.removeFromBottom(50).reduced(100, 10);
    mixSlider.setBounds(footer);
    
    // Visor (Middle) - пока пустое место под визуализацию
    auto visorArea = area.removeFromTop(area.getHeight() * 0.45f);
    
    // Main Controls (Bottom Split)
    auto controlsArea = area;
    saturationCore.setBounds(controlsArea.removeFromLeft(controlsArea.getWidth() / 2));
    networkBrain.setBounds(controlsArea);
}