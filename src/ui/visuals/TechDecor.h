#pragma once
#include "../../JuceHeader.h"
#include "../CoheraLookAndFeel.h"

class TechDecor : public juce::Component
{
public:
    TechDecor()
    {
        setInterceptsMouseClicks(false, false);
    }

    void paint(juce::Graphics& g) override
    {
        auto w = (float)getWidth();
        auto h = (float)getHeight();
        g.setColour(CoheraUI::kTextDim.withAlpha(0.15f));

        float len = 20.0f;
        float thick = 2.0f;
        g.drawLine(0, 0, len, 0, thick);
        g.drawLine(0, 0, 0, len, thick);
        g.drawLine(w, 0, w - len, 0, thick);
        g.drawLine(w, 0, w, len, thick);
        g.drawLine(0, h, len, h, thick);
        g.drawLine(0, h, 0, h - len, thick);
        g.drawLine(w, h, w - len, h, thick);
        g.drawLine(w, h, w, h - len, thick);

        g.setFont(juce::Font("Verdana", 9.0f, juce::Font::bold));
        g.drawText("CAUTION: HIGH VOLTAGE", 20, h - 50, 150, 10, juce::Justification::left);
        g.drawText("NETWORK STATUS: CONNECTED", w - 170, h - 50, 150, 10, juce::Justification::right);

        g.setFont(juce::Font(14.0f));
        g.drawText(u8"システム正常", 20, h - 35, 100, 20, juce::Justification::left);
        g.drawText(u8"音声処理中", w - 120, h - 35, 100, 20, juce::Justification::right);

        g.setColour(CoheraUI::kTextDim.withAlpha(0.05f));
        g.drawLine(w * 0.5f, 50, w * 0.5f, h - 100, 1.0f);
    }
};
