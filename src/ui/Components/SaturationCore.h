#pragma once
#include "../../JuceHeader.h"

class SaturationCore : public juce::Component
{
public:
    SaturationCore(juce::AudioProcessorValueTreeState& apvts)
    {
        // Группа
        group.setText("SATURATION CORE");
        addAndMakeVisible(group);

        // Слайдеры
        addSlider(driveSlider, driveAtt, apvts, "drive_master", "Drive");
        addSlider(tightenSlider, tightenAtt, apvts, "tone_tighten", "Tighten");
        addSlider(smoothSlider, smoothAtt, apvts, "tone_smooth", "Smooth");
        addSlider(punchSlider, punchAtt, apvts, "punch", "Punch");
        
        // Выбор Алгоритма (Math Mode или Sat Type - выбери один основной)
        // Давай используем "math_mode" как самый крутой
        algoCombo.addItemList({"Golden Ratio", "Euler Tube", "Pi Fold", "Fibonacci", "Super Ellipse"}, 1);
        addAndMakeVisible(algoCombo);
        algoAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "math_mode", algoCombo);
        
        algoLabel.setText("Algorithm", juce::dontSendNotification);
        algoLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(algoLabel);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10);
        group.setBounds(bounds);
        
        auto content = bounds.reduced(15, 25); // Отступ внутри рамки
        
        // Сетка 2x3
        auto topRow = content.removeFromTop(content.getHeight() / 2);
        
        // Drive - большой слева
        driveSlider.setBounds(topRow.removeFromLeft(topRow.getWidth() * 0.5));
        
        // Algo - справа
        auto algoArea = topRow;
        algoLabel.setBounds(algoArea.removeFromTop(20));
        algoCombo.setBounds(algoArea.reduced(10, 20));

        // Нижний ряд - Tone & Punch
        auto bottomRow = content;
        int w = bottomRow.getWidth() / 3;
        tightenSlider.setBounds(bottomRow.removeFromLeft(w));
        punchSlider.setBounds(bottomRow.removeFromLeft(w));
        smoothSlider.setBounds(bottomRow);
    }

private:
    // Хелпер для добавления слайдера
    void addSlider(juce::Slider& slider, std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>& att, 
                   juce::AudioProcessorValueTreeState& apvts, const juce::String& paramID, const juce::String& name)
    {
        slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
        slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
        addAndMakeVisible(slider);
        
        // Лейбл (можно сделать отдельным компонентом, но пока встроенный попап или просто заголовок)
        // Для простоты пока без лейблов в коде, добавим в paint или lookandfeel
        att = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(apvts, paramID, slider);
    }

    juce::GroupComponent group;
    
    juce::Slider driveSlider, tightenSlider, smoothSlider, punchSlider;
    juce::ComboBox algoCombo;
    juce::Label algoLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> driveAtt, tightenAtt, smoothAtt, punchAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> algoAtt;
};