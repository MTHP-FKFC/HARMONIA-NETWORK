#pragma once
#include "../../JuceHeader.h"
#include "../../network/NetworkManager.h"

class InteractionMeter : public juce::Component, private juce::Timer
{
public:
    InteractionMeter(juce::AudioProcessorValueTreeState& s) : apvts(s) {
        startTimerHz(30);
    }
    ~InteractionMeter() { stopTimer(); }

    void timerCallback() override { repaint(); }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat();
        float w = area.getWidth();
        float h = area.getHeight();
        float centerY = h / 2.0f;

        // Фон
        g.setColour(juce::Colours::black.withAlpha(0.3f));
        g.fillRoundedRectangle(area, 2.0f);
        g.setColour(juce::Colours::white.withAlpha(0.1f));
        g.drawHorizontalLine((int)centerY, 0, w); // Ноль

        // Данные
        int group = (int)*apvts.getRawParameterValue("group_id");
        bool isRef = *apvts.getRawParameterValue("role") > 0.5f;
        int mode = *apvts.getRawParameterValue("mode");

        if (isRef) return; // Референс не реагирует

        // Читаем общий сигнал сети (можно взять любую полосу или среднее, тут для примера Low)
        float netSignal = NetworkManager::getInstance().getBandSignal(group, 1); // Low Band

        if (netSignal < 0.02f) return;

        // Логика цвета и направления
        juce::Colour col;
        float barHeight = 0.0f;
        float startY = centerY;

        if (mode == 0) { // Unmasking (Ducking) -> Вниз
            col = juce::Colours::cyan;
            barHeight = netSignal * (h * 0.4f); // Вниз
        }
        else if (mode == 1) { // Ghost (Boost) -> Вверх
            col = juce::Colours::orange;
            barHeight = -netSignal * (h * 0.4f); // Вверх (минус Y)
        }
        else { // Gated -> Вверх (когда тихо)
             float inv = 1.0f - netSignal;
             col = juce::Colours::red;
             barHeight = -inv * (h * 0.4f);
        }

        g.setColour(col);
        // Рисуем бар от центра
        g.fillRect(0.0f, startY, w, barHeight);
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
};

class NetworkBrain : public juce::Component
{
public:
    NetworkBrain(juce::AudioProcessorValueTreeState& apvts) : meter(apvts)
    {
        group.setText("NETWORK INTELLIGENCE");
        group.setColour(juce::GroupComponent::outlineColourId, juce::Colour(0, 200, 150)); // Зеленый акцент
        group.setColour(juce::GroupComponent::textColourId, juce::Colour(0, 200, 150));
        addAndMakeVisible(group);

        // Параметры сети
        addSlider(depthSlider, depthAtt, apvts, "net_depth", "Depth");
        depthSlider.setName("Net_Depth");
        addSlider(sensSlider, sensAtt, apvts, "net_sens", "Sens");
        sensSlider.setName("Net_Sens");
        addSlider(smoothSlider, smoothAtt, apvts, "net_smooth", "Smooth");
        smoothSlider.setName("Net_Smooth");

        // Режим взаимодействия
        modeCombo.addItemList({"Unmasking", "Ghost", "Gated", "Stereo Bloom", "Sympathetic"}, 1);
        addAndMakeVisible(modeCombo);
        modeAtt = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "mode", modeCombo);

        modeLabel.setText("Interaction Mode", juce::dontSendNotification);
        modeLabel.setJustificationType(juce::Justification::centred);
        addAndMakeVisible(modeLabel);

        // Добавляем meter
        addAndMakeVisible(meter);
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

        // Meter (справа)
        auto rightEdge = content.removeFromRight(15); // Узкая полоска справа
        meter.setBounds(rightEdge.reduced(2, 10));

        // 3 Ручки в ряд (оставшееся пространство)
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
    InteractionMeter meter;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> depthAtt, sensAtt, smoothAtt;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> modeAtt;
};