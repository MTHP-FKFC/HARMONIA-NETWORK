#pragma once
#include "../JuceHeader.h"

class CoheraLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CoheraLookAndFeel()
    {
        // Цветовая палитра "Cohera Dark"
        setColour(juce::ResizableWindow::backgroundColourId, juce::Colour(20, 22, 26));
        
        // Слайдеры (Rotary)
        setColour(juce::Slider::thumbColourId, juce::Colour(255, 140, 0)); // Оранжевый акцент (Tube)
        setColour(juce::Slider::rotarySliderFillColourId, juce::Colour(255, 140, 0));
        setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(40, 44, 50));
        
        // Текст
        setColour(juce::Label::textColourId, juce::Colours::lightgrey);
        setColour(juce::GroupComponent::outlineColourId, juce::Colour(60, 65, 70));
        setColour(juce::GroupComponent::textColourId, juce::Colours::white);
    }
    
    // Можно переопределить drawRotarySlider здесь для кастомного вида,
    // но пока используем дефолтный V4 с нашими цветами.
};