#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CoheraLookAndFeel.h"

class ControlGroup : public juce::Component
{
public:
    ControlGroup(juce::String title, juce::Colour accentColor)
        : groupTitle(title), color(accentColor)
    {
        addMouseListener(this, true); // Добавляем себя как mouse listener
    }

    ~ControlGroup() override {
        removeMouseListener(this);
    }

    void mouseEnter(const juce::MouseEvent&) override {
        isHovered = true;
        repaint();
    }

    void mouseExit(const juce::MouseEvent&) override {
        isHovered = false;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto headerHeight = 25.0f;

        // Заголовок
        g.setColour(color);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(groupTitle, bounds.removeFromTop(headerHeight), juce::Justification::topLeft, true);

        // Цвет рамки зависит от ховера
        float alpha = isHovered ? 0.8f : 0.3f; // Ярче при наведении
        g.setColour(color.withAlpha(alpha));
        g.drawRoundedRectangle(bounds.expanded(-1.0f), 4.0f, isHovered ? 2.0f : 1.0f); // Толще при наведении

        // Легкий фон внутри (тоже зависит от ховера)
        float bgAlpha = isHovered ? 0.08f : 0.03f;
        g.setColour(color.withAlpha(bgAlpha));
        g.fillRoundedRectangle(bounds.expanded(-1.0f), 4.0f);
    }

private:
    juce::String groupTitle;
    juce::Colour color;
    bool isHovered = false;
};
