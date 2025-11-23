#pragma once
#include "../../JuceHeader.h"

class TopBar : public juce::Component
{
public:
    TopBar(juce::AudioProcessorValueTreeState& apvts)
    {
        // Логотип
        addAndMakeVisible(logoLabel);
        logoLabel.setText("COHERA SATURATOR", juce::dontSendNotification);
        logoLabel.setFont(juce::Font(20.0f, juce::Font::bold));
        logoLabel.setColour(juce::Label::textColourId, juce::Colours::white);

        // Group ID
        addAndMakeVisible(groupCombo);
        for(int i=0; i<8; ++i) groupCombo.addItem("Group " + juce::String(i+1), i+1);
        groupAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "group_id", groupCombo);

        // Role
        addAndMakeVisible(roleCombo);
        roleCombo.addItem("Listener", 1);
        roleCombo.addItem("Reference", 2);
        roleAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "role", roleCombo);
    }

    void paint(juce::Graphics& g) override
    {
        // Фон шапки
        g.fillAll(juce::Colour::fromRGB(15, 16, 18));
        // Линия внизу
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawLine(0, getHeight(), getWidth(), getHeight(), 1.0f);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced(10, 5);
        
        logoLabel.setBounds(area.removeFromLeft(200));
        
        // Элементы справа
        auto rightArea = area.removeFromRight(250);
        roleCombo.setBounds(rightArea.removeFromRight(100).reduced(0, 5));
        rightArea.removeFromRight(10); // gap
        groupCombo.setBounds(rightArea.removeFromRight(80).reduced(0, 5));
    }

private:
    juce::Label logoLabel;
    juce::ComboBox groupCombo, roleCombo;
    
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> groupAtt, roleAtt;
};