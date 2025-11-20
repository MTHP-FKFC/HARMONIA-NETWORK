#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "CoheraLookAndFeel.h"

class ControlGroup : public juce::Component
{
public:
    ControlGroup(juce::String title, juce::Colour accentColor)
        : groupTitle(title), color(accentColor) {}

    void paint(juce::Graphics& g) override
    {
        auto bounds = getLocalBounds().toFloat();
        auto headerHeight = 25.0f;

        // Заголовок
        g.setColour(color);
        g.setFont(juce::Font(14.0f, juce::Font::bold));
        g.drawText(groupTitle, bounds.removeFromTop(headerHeight), juce::Justification::topLeft, true);

        // Рамка
        g.setColour(color.withAlpha(0.3f));
        g.drawRoundedRectangle(bounds.expanded(-1.0f), 4.0f, 1.0f);

        // Легкий фон внутри
        g.setColour(color.withAlpha(0.03f));
        g.fillRoundedRectangle(bounds.expanded(-1.0f), 4.0f);
    }

private:
    juce::String groupTitle;
    juce::Colour color;
};
