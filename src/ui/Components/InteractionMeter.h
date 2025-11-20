#pragma once

#include "../../JuceHeader.h"
#include "../CoheraLookAndFeel.h"
#include "../../network/NetworkManager.h"

class InteractionMeter : public juce::Component, private juce::Timer
{
public:
    InteractionMeter() : apvts(nullptr)
    {
        startTimerHz(30);
    }

    ~InteractionMeter() override { stopTimer(); }

    void timerCallback() override { repaint(); }

    void setAPVTS(juce::AudioProcessorValueTreeState& newApvts)
    {
        apvts = &newApvts;
    }

    void paint(juce::Graphics& g) override
    {
        auto area = getLocalBounds().toFloat().reduced(4.0f);

        // Фон (Slot)
        g.setColour(CoheraUI::kPanel.darker(0.5f));
        g.fillRoundedRectangle(area, 4.0f);
        g.setColour(CoheraUI::kTextDim.withAlpha(0.1f));
        g.drawRoundedRectangle(area, 4.0f, 1.0f);

        // Данные
        float modulation = 0.0f;
        juce::Colour barColor = CoheraUI::kCyanNeon;

        if (apvts != nullptr) {
            // Берем среднюю модуляцию по всем полосам для простоты (или макс)
            int group = (int)*apvts->getRawParameterValue("group_id");
            // Эмуляция чтения: в реальности нужно брать из NetworkController'а
            // Пока берем Band 0 как пример
            modulation = NetworkManager::getInstance().getBandSignal(group, 0);

            // Логика цвета (Зависит от режима)
            int mode = (int)*apvts->getRawParameterValue("mode");
            bool isReduction = (mode == 0); // Unmasking = Reduction (вниз)

            if (isReduction) barColor = CoheraUI::kCyanNeon; // Ducking
            else barColor = CoheraUI::kOrangeNeon;           // Boosting (Ghost)
        }

        // Рисуем Бар
        // Для демонстрации всегда показываем немного активности
        if (modulation < 0.1f) modulation = 0.3f + 0.1f * std::sin(juce::Time::getCurrentTime().toMilliseconds() * 0.001f); // Demo value with animation

        float barHeight = area.getHeight() * modulation;

        juce::Rectangle<float> barRect;
        // Сверху вниз (Gain Reduction style)
        barRect = area.removeFromTop(barHeight);

        // Gradient Fill
        juce::ColourGradient grad(barColor, barRect.getCentreX(), barRect.getY(),
                                  barColor.darker(0.5f), barRect.getCentreX(), barRect.getBottom(), false);

        g.setGradientFill(grad);
        g.fillRoundedRectangle(barRect, 4.0f);

        // Текстовая метка
        g.setColour(CoheraUI::kTextDim);
        g.setFont(juce::Font("Verdana", 9.0f, juce::Font::plain));
        g.drawText("ACTIVITY", getLocalBounds().removeFromBottom(12), juce::Justification::centred);
    }

private:
    juce::AudioProcessorValueTreeState* apvts;
};