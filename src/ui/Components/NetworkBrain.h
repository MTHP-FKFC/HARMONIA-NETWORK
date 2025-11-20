#pragma once
#include "../../JuceHeader.h"

class NetworkBrain : public juce::Component
{
public:
    NetworkBrain(juce::AudioProcessorValueTreeState& apvts)
    {
        group.setText("NETWORK INTELLIGENCE");
        group.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0, 200, 150)); // Зеленый акцент
        group.setColour(juce::GroupComponent::textColourId, juce::Colour(0, 200, 150));
        addAndMakeVisible(group);

        // Параметры сети
        addSlider(depthSlider, depthAtt, apvts, "net_depth", "Depth");
        addSlider(sensSlider, sensAtt, apvts, "net_sens", "Sens");
        addSlider(smoothSlider, smoothAtt, apvts, "net_smooth", "Smooth");
        
        // Режим взаимодействия
        modeCombo.addItemList({"Unmasking", "Ghost", "Gated", "Stereo Bloom", "Sympathetic"}, 1);
        addAndMakeVisible(modeCombo);
        modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "mode", modeCombo);
        
        modeLabel.setText("Interaction Mode", juce::dontSendNotification);
        modeLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(modeLabel);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        group.setBounds(bounds);
        auto content = bounds.reduced(15, 25);

        // Mode selector (Top)
        auto topSection = content.removeFromTop(50);
        modeLabel.setBounds(topSection.removeFromTop(20));
        modeCombo.setBounds(topSection.reduced(20, 0));

        content.removeFromTop(10); // Gap

        // 3 Ручки в ряд
        int w = content.getWidth() / 3;
        sensSlider.setBounds(content.removeFromLeft(w));
        depthSlider.setBounds(content.removeFromLeft(w));
        smoothSlider.setBounds(content);
    }

private:
    void addSlider(juce::Slider& slider, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att, 
                   juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& name)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, slider);
    }

    juce::GroupComponent group;
    juce::Slider depthSlider, sensSlider, smoothSlider;
    juce::ComboBox modeCombo;
    juce::Label modeLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAtt, sensAtt, smoothAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;
};