#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
CoheraSaturatorAudioProcessorEditor::CoheraSaturatorAudioProcessorEditor (CoheraSaturatorAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p)
{
    // 1. Применяем LookAndFeel
    lookAndFeel = std::make_unique<CoheraUI::CoheraLookAndFeel>();
    setLookAndFeel(lookAndFeel.get());

    // 2. Создаем компоненты

    // --- TOP BAR ---
    addAndMakeVisible(groupSelector);
    groupSelector.addItemList({"Group 1", "Group 2", "Group 3", "Group 4", "Group 5", "Group 6", "Group 7", "Group 8"}, 1);
    groupSelector.setSelectedId(1);
    groupAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.getAPVTS(), "group_id", groupSelector);

    addAndMakeVisible(roleSelector);
    roleSelector.addItemList({"Listener", "Reference"}, 1);
    roleSelector.setSelectedId(1);
    roleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.getAPVTS(), "role", roleSelector);

    // Quality Selector
    addAndMakeVisible(qualitySelector);
    qualitySelector.addItemList({"Eco Mode", "Pro (4x)"}, 1);
    qualityAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.getAPVTS(), "quality", qualitySelector);

    // --- VISOR ---
    addAndMakeVisible(spectrumVisor);

    // --- SATURATION CORE (Left) ---
    addAndMakeVisible(satGroup);

    // Algorithm Selector
    addAndMakeVisible(mathModeSelector);
    mathModeSelector.addItemList(juce::StringArray{
        // === DIVINE SERIES ===
        "Golden Ratio",
        "Euler Tube",
        "Pi Fold",
        "Fibonacci",
        "Super Ellipse",
        // === CLASSIC SERIES ===
        "Analog Tape",
        "Vintage Console",
        "Diode Class A",
        "Tube Driver",
        "Digital Fuzz",
        "Bit Decimator",
        "Rectifier"
    }, 1);
    mathModeSelector.setSelectedId(1);
    mathModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.getAPVTS(), "math_mode", mathModeSelector);

    // Cascade Button - Output Limiter Stage
    addAndMakeVisible(cascadeButton);
    cascadeButton.setButtonText("CASCADE");
    cascadeButton.setClickingTogglesState(true);
    cascadeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::orange.withAlpha(0.6f));
    cascadeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.getAPVTS(), "cascade", cascadeButton);

    // Drive Big Knob
    addAndMakeVisible(driveSlider);
    driveSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    driveSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    driveSlider.setColour(juce::Slider::thumbColourId, CoheraUI::kOrangeNeon);
    driveSlider.setName("DRIVE"); // Для правильного выбора цвета в drawRotarySlider
    driveAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "drive_master", driveSlider);

    // Dynamics Attachment
    dynamicsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "dynamics", dynamicsSlider);

    // Output Attachment
    outputAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "output_gain", outputSlider);

    // Focus Attachment
    focusAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "focus", focusSlider);

    // Mojo Attachments
    heatAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "heat", heatSlider);
    driftAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "analog_drift", driftSlider);
    varianceAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "variance", varianceSlider);
    entropyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "entropy", entropySlider);
    noiseAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(p.getAPVTS(), "noise", noiseSlider);

    // Tone Knobs
    setupKnob(tightenSlider, "tone_tighten", "TIGHTEN", CoheraUI::kOrangeNeon);
    setupKnob(punchSlider, "punch", "PUNCH", CoheraUI::kOrangeNeon);
    setupKnob(smoothSlider, "tone_smooth", "SMOOTH", CoheraUI::kOrangeNeon);

    // --- NETWORK BRAIN (Right) ---
    addAndMakeVisible(netGroup);

    // Network Mode Selector
    addAndMakeVisible(netModeSelector);
    netModeSelector.addItemList(juce::StringArray{
        "Unmasking (Duck)",
        "Ghost (Follow)",
        "Gated (Reverse)",
        "Stereo Bloom",
        "Sympathetic"
    }, 1);
    netModeSelector.setSelectedId(1);
    netModeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.getAPVTS(), "mode", netModeSelector);

    // Network Knobs
    setupKnob(netSensSlider, "net_sens", "SENS", CoheraUI::kCyanNeon);
    setupKnob(netDepthSlider, "net_depth", "DEPTH", CoheraUI::kCyanNeon);
    setupKnob(netSmoothSlider, "net_smooth", "RELEASE", CoheraUI::kCyanNeon);

    // Dynamics Knob
    setupKnob(dynamicsSlider, "dynamics", "DYNAMICS", CoheraUI::kOrangeNeon);

    // Output Knob
    setupKnob(outputSlider, "output_gain", "OUTPUT", CoheraUI::kTextBright);

    // Focus Knob
    setupKnob(focusSlider, "focus", "FOCUS", CoheraUI::kTextBright);

    // Mojo Knobs
    setupKnob(heatSlider, "heat", "HEAT", CoheraUI::kOrangeNeon);
    setupKnob(driftSlider, "drift", "DRIFT", CoheraUI::kCyanNeon);
    setupKnob(varianceSlider, "variance", "VAR", CoheraUI::kOrangeNeon);
    setupKnob(entropySlider, "entropy", "ENTROPY", CoheraUI::kOrangeNeon);
    setupKnob(noiseSlider, "noise", "NOISE", CoheraUI::kRedNeon);

    // Interaction Meter - temporarily disabled
    // interactionMeter.setAPVTS(p.getAPVTS());
    // addAndMakeVisible(interactionMeter);

    // --- BOTTOM MIX ---
    setupKnob(mixSlider, "mix", "MIX", CoheraUI::kTextBright);

    // Delta Button
    addAndMakeVisible(deltaButton);
    deltaButton.setButtonText(u8"Δ");
    deltaButton.setClickingTogglesState(true);
    deltaButton.setColour(juce::TextButton::buttonOnColourId, juce::Colours::yellow.withAlpha(0.6f));
    deltaAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(p.getAPVTS(), "delta", deltaButton);

    // Базовый размер
    setSize (900, 650);
    setResizable(true, true);
    setResizeLimits(600, 400, 1920, 1080);
}

CoheraSaturatorAudioProcessorEditor::~CoheraSaturatorAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
    lookAndFeel.reset();
}

// Хелпер для быстрой настройки ручек
void CoheraSaturatorAudioProcessorEditor::setupKnob(juce::Slider& s, juce::String paramId, juce::String displayName, juce::Colour c)
{
    addAndMakeVisible(s);
    s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    s.setColour(juce::Slider::thumbColourId, c);

    // ВАЖНО: Устанавливаем имя для отображения
    s.setName(displayName);

    // Храним аттачменты в векторе, чтобы не создавать кучу named variables
    sliderAttachments.push_back(std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(audioProcessor.getAPVTS(), paramId, s));
}

void CoheraSaturatorAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (CoheraUI::kBackground);

    // Top Bar BG
    g.setColour(CoheraUI::kPanel);
    g.fillRect(0, 0, getWidth(), 50);

    // Logo
    g.setColour(CoheraUI::kTextBright);
    g.setFont(juce::Font("Verdana", 20.0f, juce::Font::bold));
    g.drawText("COHERA SATURATOR", 20, 0, 200, 50, juce::Justification::centredLeft);
}

void CoheraSaturatorAudioProcessorEditor::resized()
{
    auto area = getLocalBounds();

    // 1. Глобальные отступы (Padding)
    // "Воздух" по краям — признак дорогого дизайна
    area.reduce(16, 16);

    // ==============================================================================
    // 🔝 HEADER & VISOR (35% высоты)
    // ==============================================================================
    auto topSection = area.removeFromTop(static_cast<int>(getHeight() * 0.38f));

    // Top Bar (Selectors) - 40px height fixed inside proportional area
    auto topBar = topSection.removeFromTop(40);

    // Right Align selectors: Role -> Group -> Quality
    roleSelector.setBounds(topBar.removeFromRight(100));
    topBar.removeFromRight(10); // Spacer
    groupSelector.setBounds(topBar.removeFromRight(80));
    topBar.removeFromRight(10); // Spacer
    qualitySelector.setBounds(topBar.removeFromRight(90));

    topSection.removeFromTop(10); // Spacer to Visor

    // Visor занимает всё оставшееся место в топе
    spectrumVisor.setBounds(topSection);

    area.removeFromTop(16); // Spacer между Визором и Панелями

    // ==============================================================================
    // 🦶 FOOTER (18% высоты) - Снизу вверх
    // ==============================================================================
    auto footerHeight = static_cast<int>(getHeight() * 0.20f); // Увеличено для больших Mojo ручек
    auto footerArea = area.removeFromBottom(footerHeight);
    layoutFooter(footerArea);

    area.removeFromBottom(16); // Spacer перед футером

    // ==============================================================================
    // 🎛️ MAIN PANELS (Оставшееся место по центру)
    // ==============================================================================

    // Делим пополам с отступом посередине
    auto leftPanel = area.removeFromLeft(area.getWidth() / 2).reduced(4, 0); // Чуть сужаем для gap
    auto rightPanel = area.reduced(4, 0); // Оставшееся справа

    // Устанавливаем границы Групп (Рамки)
    satGroup.setBounds(leftPanel);
    netGroup.setBounds(rightPanel);

    // Заполняем внутренности групп (с учетом отступа под заголовок группы)
    // Отступ сверху 30px под текст "SATURATION CORE"
    layoutSaturation(leftPanel.reduced(12, 12).withTrimmedTop(25));
    layoutNetwork(rightPanel.reduced(12, 12).withTrimmedTop(25));
}

// --- ХЕЛПЕР: Раскладка Сатурации ---
void CoheraSaturatorAudioProcessorEditor::layoutSaturation(juce::Rectangle<int> area)
{
    // Верхняя половина: Drive (King) + Control Bar (Algo + Cascade)
    auto topHalf = area.removeFromTop(area.getHeight() * 0.55f);

    // Drive Knob - Главный герой, по центру левой части
    // Занимает 55% ширины (чуть меньше, чтобы влезли селекторы)
    auto driveArea = topHalf.removeFromLeft(topHalf.getWidth() * 0.55f);
    driveSlider.setBounds(driveArea.reduced(5)); // reduced, чтобы не касаться краев

    // Справа от Драйва: Control Bar (Algo + Cascade)
    auto controlBar = topHalf;

    // Делим вертикально пополам с отступом
    int controlHeight = 24;
    int gap = 8;
    int totalH = controlHeight * 2 + gap;
    int startY = (controlBar.getHeight() - totalH) / 2;

    auto controlRect = controlBar.reduced(5, 0);
    controlRect.setY(controlBar.getY() + startY);
    controlRect.setHeight(totalH);

    mathModeSelector.setBounds(controlRect.removeFromTop(controlHeight)); // Algo
    controlRect.removeFromTop(gap);
    cascadeButton.setBounds(controlRect.removeFromTop(controlHeight));  // Cascade Button

    // Нижняя половина: 4 ручки тона в ряд (Tighten, Punch, Dyn, Smooth)
    // Используем FlexBox для идеального распределения
    juce::FlexBox toneFlex;
    toneFlex.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;

    // Массив ручек для добавления
    juce::Slider* knobs[] = { &tightenSlider, &punchSlider, &dynamicsSlider, &smoothSlider };

    for (auto* k : knobs) {
        toneFlex.items.add(juce::FlexItem(*k).withFlex(1.0f).withMargin(2.0f));
    }

    toneFlex.performLayout(area.reduced(0, 5)); // Немного воздуха сверху/снизу
}

// --- ХЕЛПЕР: Раскладка Сети ---
void CoheraSaturatorAudioProcessorEditor::layoutNetwork(juce::Rectangle<int> area)
{
    // Верх: Режим сети (Mode)
    auto headerArea = area.removeFromTop(40);
    netModeSelector.setBounds(headerArea.withSizeKeepingCentre(headerArea.getWidth() - 20, 24));

    // Оставшееся делим: Слева ручки, Справа Метр
    // Метр занимает 15% ширины справа - temporarily disabled
    // auto meterArea = area.removeFromRight(area.getWidth() * 0.15f).reduced(5, 10);
    // interactionMeter.setBounds(meterArea);

    // Ручки: 3 штуки треугольником или в ряд
    // Сделаем треугольник для разнообразия (Sens сверху, Depth/Smooth снизу)
    auto knobArea = area.reduced(5, 0);

    auto topKnobRow = knobArea.removeFromTop(knobArea.getHeight() / 2);
    netSensSlider.setBounds(topKnobRow.withSizeKeepingCentre(topKnobRow.getHeight(), topKnobRow.getHeight()));

    // Нижний ряд (Depth, Smooth)
    juce::FlexBox netFlex;
    netFlex.justifyContent = juce::FlexBox::JustifyContent::spaceAround;
    netFlex.items.add(juce::FlexItem(netDepthSlider).withFlex(1.0f));
    netFlex.items.add(juce::FlexItem(netSmoothSlider).withFlex(1.0f));

    netFlex.performLayout(knobArea);
}

// --- ХЕЛПЕР: Футер (Mix & Mojo) ---
void CoheraSaturatorAudioProcessorEditor::layoutFooter(juce::Rectangle<int> area)
{
    // Простое разделение на 3 равные части
    int sectionWidth = area.getWidth() / 3;

    auto leftSection = area.removeFromLeft(sectionWidth);
    auto centerSection = area.removeFromLeft(sectionWidth);
    auto rightSection = area; // Оставшееся

    // === 1. MOJO RACK (Left) ===
    // 5 ручек в ряд: Heat, Drift, Variance, Entropy, Noise
    juce::FlexBox mojoFlex;
    mojoFlex.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;

    juce::Slider* mojoKnobs[] = { &heatSlider, &driftSlider, &varianceSlider, &entropySlider, &noiseSlider };

    for (auto* k : mojoKnobs) {
        mojoFlex.items.add(juce::FlexItem(*k).withFlex(1.0f).withMargin(juce::FlexItem::Margin(0, 2, 0, 2)));
    }
    mojoFlex.performLayout(leftSection.reduced(0, 5));

    // === 2. MIX CENTER ===
    // Mix Knob
    mixSlider.setBounds(centerSection.withSizeKeepingCentre(70, 70));

    // Delta Button (Маленькая кнопка рядом с Mix)
    int btnSize = 20;
    deltaButton.setBounds(mixSlider.getRight() - 10, mixSlider.getY(), btnSize, btnSize);

    // === 3. OUTPUT SECTION (Right) ===
    // Focus и Output рядом
    juce::FlexBox outFlex;
    outFlex.justifyContent = juce::FlexBox::JustifyContent::flexEnd;

    outFlex.items.add(juce::FlexItem(focusSlider).withFlex(1.0f).withMaxWidth(70).withMargin(5));
    outFlex.items.add(juce::FlexItem(outputSlider).withFlex(1.0f).withMaxWidth(70).withMargin(5));

    outFlex.performLayout(rightSection.reduced(0, 5));
}