#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace CoheraUI {

    // === НЕОНОВАЯ ПАЛИТРА (Cyber-Dark) ===
    const juce::Colour kBackground   = juce::Colour::fromFloatRGBA(0.07f, 0.07f, 0.08f, 1.0f); // Deep Space
    const juce::Colour kPanel        = juce::Colour::fromFloatRGBA(0.11f, 0.11f, 0.13f, 1.0f); // Matte Metal
    const juce::Colour kPanelLight   = juce::Colour::fromFloatRGBA(0.16f, 0.16f, 0.18f, 1.0f); // Highlight

    const juce::Colour kOrangeNeon   = juce::Colour::fromFloatRGBA(1.0f, 0.6f, 0.0f, 1.0f);    // Drive Glow
    const juce::Colour kCyanNeon     = juce::Colour::fromFloatRGBA(0.0f, 0.9f, 1.0f, 1.0f);    // Net Glow
    const juce::Colour kRedNeon      = juce::Colour::fromFloatRGBA(1.0f, 0.2f, 0.3f, 1.0f);    // Mute

    const juce::Colour kTextBright   = juce::Colours::white.withAlpha(0.95f);
    const juce::Colour kTextDim      = juce::Colours::white.withAlpha(0.4f);

    class CoheraLookAndFeel : public juce::LookAndFeel_V4
    {
    public:
        CoheraLookAndFeel()
        {
            // Настраиваем шрифты (футуристичный стиль)
            setDefaultSansSerifTypefaceName("Verdana");
        }

        // ========================================================================
        // 🎛️ ULTRA-REALISTIC KNOB DRAWING WITH INTEGRATED LABELS
        // ========================================================================
        void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                              float sliderPos, const float rotaryStartAngle,
                              const float rotaryEndAngle, juce::Slider& slider) override
        {
            auto bounds = juce::Rectangle<float>(x, y, width, height).reduced(2.0f);
            auto center = bounds.getCentre();
            float radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;

            // --- 1. ЦВЕТОВАЯ ЛОГИКА ---
            juce::Colour mainColor = kOrangeNeon; // Default Saturation

            if (slider.getName().containsIgnoreCase("Net") || slider.getName().containsIgnoreCase("Ghost")) {
                mainColor = kCyanNeon;
            }
            else if (slider.getName().containsIgnoreCase("Punch")) {
                mainColor = juce::Colour::fromFloatRGBA(1.0f, 0.3f, 0.5f, 1.0f); // Hot Pink for Punch
            }
            else if (slider.getName().containsIgnoreCase("Mix")) {
                mainColor = kTextBright; // White for Mix
            }

            // --- 2. ГЕОМЕТРИЯ ---
            float toAngle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
            radius -= 10.0f; // <--- ВАЖНО: Отступ внутрь на 10px со всех сторон для текста
            float arcRadius = radius * 0.80f; // Чуть меньше, чтобы влез текст
            float knobRadius = radius * 0.60f;

            // --- 3. ОТРИСОВКА ФОНА (TRACK) ---
            juce::Path backgroundArc;
            backgroundArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, rotaryEndAngle, true);
            g.setColour(kPanelLight.darker(0.1f));
            g.strokePath(backgroundArc, juce::PathStrokeType(4.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // --- 4. ОТРИСОВКА ЗНАЧЕНИЯ (NEON GLOW ARC) ---
            juce::Path valueArc;
            valueArc.addCentredArc(center.x, center.y, arcRadius, arcRadius, 0.0f, rotaryStartAngle, toAngle, true);

            // Glow (Широкий, прозрачный)
            g.setColour(mainColor.withAlpha(0.2f));
            g.strokePath(valueArc, juce::PathStrokeType(10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // Core (Узкий, яркий)
            g.setColour(mainColor);
            g.strokePath(valueArc, juce::PathStrokeType(3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            // --- 5. ТЕЛО РУЧКИ (MATTE METAL) ---
            juce::ColourGradient knobGrad(
                kPanelLight, center.x, center.y - knobRadius,
                kPanel.darker(0.4f), center.x, center.y + knobRadius, false);

            g.setGradientFill(knobGrad);
            g.fillEllipse(center.x - knobRadius, center.y - knobRadius, knobRadius * 2, knobRadius * 2);

            // Тонкий ободок (Specular Highlight)
            g.setColour(juce::Colours::white.withAlpha(0.1f));
            g.drawEllipse(center.x - knobRadius + 1, center.y - knobRadius + 1, (knobRadius * 2) - 2, (knobRadius * 2) - 2, 1.0f);

            // --- 6. POINTER (УКАЗАТЕЛЬ) ---
            juce::Path p;
            float pointerH = knobRadius * 0.3f;
            p.addRectangle(-1.5f, -knobRadius + 4, 3.0f, pointerH);
            p.applyTransform(juce::AffineTransform::rotation(toAngle).translated(center));

            g.setColour(mainColor.brighter(0.5f)); // Почти белый, но с оттенком
            g.fillPath(p);

            // Свет от поинтера
            g.setColour(mainColor.withAlpha(0.6f));
            g.strokePath(p, juce::PathStrokeType(2.0f));

            // ====================================================================
            // ✍️ ТИПОГРАФИКА "DESIGNER 20 YEARS EXP"
            // ====================================================================

            // 1. НАЗВАНИЕ ПАРАМЕТРА (Снизу)
            // Прием: All Caps + Wide Tracking (разрядка) + Small Size + Dim Color
            juce::Font nameFont("Verdana", 10.0f, juce::Font::bold);
            nameFont.setExtraKerningFactor(0.15f); // << Секрет дорогого текста

            juce::String name = slider.getName().toUpperCase();

            // Очищаем имя от префиксов (tone_tighten -> TIGHTEN)
            if (name.contains("_")) name = name.substring(name.indexOf("_") + 1);
            if (name == "SENS") name = "SENSITIVITY"; // Полные имена выглядят дороже

            g.setFont(nameFont);
            g.setColour(kTextDim);

            // Позиционирование текста (Имени)
            // Рисуем НИЖЕ круга, используя bounds
            juce::Rectangle<int> nameRect(
                bounds.getX(),
                center.y + radius + 5.0f, // 5px от нижнего края круга
                bounds.getWidth(),
                20
            );

            g.setFont(nameFont);
            g.drawFittedText(name, nameRect, juce::Justification::centredTop, 1);

            // 2. ТЕКУЩЕЕ ЗНАЧЕНИЕ (Внутри ручки или сверху при наведении)
            // Рисуем значение ВНУТРИ ручки, если она большая, или СВЕРХУ, если маленькая
            bool isHovered = slider.isMouseOverOrDragging();

            if (isHovered || slider.isMouseButtonDown())
            {
                juce::String valText = slider.getTextFromValue(slider.getValue());

                // Если ручка большая (Drive), рисуем внутри
                if (radius > 30.0f) {
                    g.setColour(kTextBright);
                    g.setFont(juce::Font(14.0f, juce::Font::bold));
                    g.drawText(valText, center.x - 30, center.y - 10, 60, 20, juce::Justification::centred);
                }
                // Иначе рисуем сверху (Popup effect)
                else {
                    g.setColour(mainColor);
                    g.setFont(juce::Font(11.0f, juce::Font::bold));
                    g.drawText(valText, x, y, width, 15, juce::Justification::centred);
                }
            }
        }

        // ========================================================================
        // 🎚️ COMBO BOX (Стеклянный стиль)
        // ========================================================================
        void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                           int, int, int, int, juce::ComboBox& box) override
        {
            auto cornerSize = 4.0f;
            juce::Rectangle<float> bounds(0.5f, 0.5f, width - 1.0f, height - 1.0f);

            // Фон (Темное стекло)
            g.setColour(kPanel.darker(0.3f));
            g.fillRoundedRectangle(bounds, cornerSize);

            // Градиент при нажатии
            if (isButtonDown) {
                g.setColour(kOrangeNeon.withAlpha(0.1f));
                g.fillRoundedRectangle(bounds, cornerSize);
            }

            // Рамка (Тонкая)
            g.setColour(kTextDim.withAlpha(0.3f));
            g.drawRoundedRectangle(bounds, cornerSize, 1.0f);

            // Стрелка
            auto arrowZone = bounds.removeFromRight(height).reduced(height * 0.3f);
            juce::Path path;
            path.startNewSubPath (arrowZone.getX(), arrowZone.getY());
            path.lineTo (arrowZone.getCentreX(), arrowZone.getBottom());
            path.lineTo (arrowZone.getRight(), arrowZone.getY());

            g.setColour (box.isEnabled() ? kTextDim : kTextDim.withAlpha (0.3f));
            g.strokePath (path, juce::PathStrokeType (1.5f));
        }

        // Текст комбобокса
        void drawComboBoxTextWhenNothingSelected (juce::Graphics& g, juce::ComboBox&, juce::Label& label) override
        {
            label.setJustificationType(juce::Justification::centredLeft);
            g.setFont(juce::Font("Verdana", 13.0f, juce::Font::plain));
            g.setColour(kTextBright);
        }

        // ========================================================================
        // 🔘 BUTTONS (Tactile Flat)
        // ========================================================================
        void drawButtonBackground(juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                  bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
        {
            auto bounds = button.getLocalBounds().toFloat().reduced(1.0f);
            bool isToggle = button.getClickingTogglesState();
            bool isOn = button.getToggleState();

            // Базовый цвет
            juce::Colour baseColor = kPanelLight;

            // Логика цветов для Mute/Solo
            if (button.getName() == "S") {
                if (isOn) baseColor = juce::Colours::yellow.darker(0.2f);
            }
            else if (button.getName() == "M") {
                if (isOn) baseColor = kRedNeon.darker(0.2f);
            }
            else if (isOn && isToggle) {
                baseColor = kOrangeNeon.darker(0.3f); // Обычная активная кнопка
            }

            // Ховер эффект (подсветка)
            if (shouldDrawButtonAsHighlighted && !shouldDrawButtonAsDown)
                baseColor = baseColor.brighter(0.1f);

            g.setColour(baseColor);
            g.fillRoundedRectangle(bounds, 4.0f);

            // Если нажата (в момент клика)
            if (shouldDrawButtonAsDown) {
                g.setColour(juce::Colours::black.withAlpha(0.2f));
                g.fillRoundedRectangle(bounds, 4.0f);
            }

            // Текст
            g.setColour(isOn ? juce::Colours::black : kTextBright);
            g.setFont(juce::Font(12.0f, isOn ? juce::Font::bold : juce::Font::plain));
            g.drawText(button.getButtonText(), bounds, juce::Justification::centred);
        }
    };

} // namespace CoheraUI