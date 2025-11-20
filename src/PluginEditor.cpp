#include "PluginEditor.h"

CoheraSaturatorAudioProcessorEditor::CoheraSaturatorAudioProcessorEditor(CoheraSaturatorAudioProcessor& p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      saturationCore(p.getAPVTS()),
      networkBrain(p.getAPVTS()),
      topBar(p.getAPVTS()),
      spectrumVisor(p.getAnalyzer(), p.getAPVTS())
{
    setLookAndFeel(&lnf);

    addAndMakeVisible(topBar);
    addAndMakeVisible(spectrumVisor);
    addAndMakeVisible(saturationCore);
    addAndMakeVisible(networkBrain);
    
    // Mix knob (внизу по центру)
    mixSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    mixSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    addAndMakeVisible(mixSlider);
    mixAtt = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "mix", mixSlider);

    setSize(900, 650); // Чуть шире для удобства
    setResizable(true, true);
}

CoheraSaturatorAudioProcessorEditor::~CoheraSaturatorAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void CoheraSaturatorAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(findColour(juce::ResizableWindow::backgroundColourId));
}

void CoheraSaturatorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();
    
    topBar.setBounds(area.removeFromTop(50));
    
    // Footer (Mix)
    auto footer = area.removeFromBottom(60);
    mixSlider.setBounds(footer.getCentreX() - 30, footer.getY(), 60, 60);

    // Main area
    auto visorArea = area.removeFromTop(250); // Большой экран
    spectrumVisor.setBounds(visorArea.reduced(10, 0));
    
    auto controls = area.reduced(10);
    saturationCore.setBounds(controls.removeFromLeft(controls.getWidth() / 2).reduced(5));
    networkBrain.setBounds(controls.reduced(5));
}