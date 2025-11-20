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

    // --- ENERGY LINK ---
    addAndMakeVisible(energyLink);

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

    // Drive Big Knob (Reactor Knob with RMS animation)
    addAndMakeVisible(driveSlider);
    driveSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    driveSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    driveSlider.setColour(juce::Slider::thumbColourId, CoheraUI::kOrangeNeon);
    driveSlider.setName("DRIVE"); // Для правильного выбора цвета в drawRotarySlider

    // Подключаем RMS источник для анимации реактора
    driveSlider.setRMSGetter([this]() { return audioProcessor.getProcessingEngine().getInputRMS(); });

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

    // Net Saturation Selector
    addAndMakeVisible(netSatSelector);
    netSatSelector.addItemList({"Clean Gain", "Drive Boost", "Rectify", "Bit Crush"}, 1);
    netSatAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(p.getAPVTS(), "net_reaction", netSatSelector);

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

    // === NEW VISUAL SYSTEM v2.0 ===
    // TEST: Enable CosmicDust + NeuralLink
    addAndMakeVisible(cosmicDust);
    cosmicDust.toBack(); // Фон всегда сзади

    addAndMakeVisible(neuralLink); // NeuralLink уже инициализирован с apvts
    addAndMakeVisible(shaperScope);

    // Инициализируем APVTS для визуальных компонентов
    // neuralLink.setAPVTS(audioProcessor.getAPVTS());

    // Запускаем таймер для анимаций (20 FPS для визуализаторов)
    startTimerHz(20);

    // Базовый размер
    setSize (900, 650);
    setResizable(true, true);
    setResizeLimits(600, 400, 1920, 1080);
}

CoheraSaturatorAudioProcessorEditor::~CoheraSaturatorAudioProcessorEditor()
{
    stopTimer();
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
    auto area = getLocalBounds().toFloat();

    // 1. Базовый темный фон
    g.fillAll(CoheraUI::kBackground);

    // 2. Радиальный градиент (Vignette) - Свет в центре, тьма по краям
    // Создает фокус на центре интерфейса
    juce::ColourGradient vignette(
        CoheraUI::kBackground.brighter(0.05f), area.getCentreX(), area.getCentreY(),
        juce::Colours::black, 0, 0, true);

    g.setGradientFill(vignette);
    g.fillAll();

    // 3. Top Bar с тенью
    g.setColour(CoheraUI::kPanel);
    g.fillRect(0, 0, getWidth(), 50);

    // Тень под шапкой
    juce::ColourGradient shadow(
        juce::Colours::black.withAlpha(0.5f), 0, 50,
        juce::Colours::transparentBlack, 0, 60, false);
    g.setGradientFill(shadow);
    g.fillRect(0, 50, getWidth(), 10);

    // Логотип с легким свечением
    g.setColour(CoheraUI::kTextBright);
    g.setFont(juce::Font("Verdana", 20.0f, juce::Font::bold));
    g.drawText("COHERA", 20, 0, 200, 50, juce::Justification::centredLeft);

    // Вторая часть логотипа другим весом шрифта
    g.setColour(CoheraUI::kOrangeNeon);
    g.setFont(juce::Font("Verdana", 20.0f, juce::Font::plain));
    g.drawText("SATURATOR", 110, 0, 200, 50, juce::Justification::centredLeft);
}

// Timer callback для обновления живых визуализаторов
void CoheraSaturatorAudioProcessorEditor::timerCallback()
{
    // TEST: Enable only CosmicDust updates
    if (!isVisible()) return;

    float inputRMS = audioProcessor.getInputRMS();
    float outputRMS = audioProcessor.getOutputRMS();

    cosmicDust.setEnergyLevel(outputRMS);
    neuralLink.setEnergyLevel(inputRMS);

    // FULL FUNCTIONALITY: Enable TransferFunctionDisplay with all parameters
    auto& apvts = audioProcessor.getAPVTS();
    if (auto* driveParam = apvts.getRawParameterValue("drive_master"))
    {
        float drive = *driveParam;

        Cohera::SaturationMode mathMode = Cohera::SaturationMode::GoldenRatio;
        if (auto* mathModeParam = apvts.getRawParameterValue("math_mode"))
        {
            mathMode = static_cast<Cohera::SaturationMode>((int)*mathModeParam);
        }

        bool cascade = false;
        if (auto* cascadeParam = apvts.getRawParameterValue("cascade"))
        {
            cascade = *cascadeParam > 0.5f;
        }

        shaperScope.setParameters(drive, mathMode, inputRMS);
        shaperScope.setCascadeMode(cascade);
        shaperScope.setEnergyLevel(outputRMS);
    }
}

void CoheraSaturatorAudioProcessorEditor::resized()
{
    // Cosmic Dust - всегда на весь экран, позади всего
    cosmicDust.setBounds(getLocalBounds());

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

    // Делим на 3 части: Left Panel | Link (Gap) | Right Panel
    auto centerGap = area.getWidth() * 0.12f; // 12% ширины на связку - увеличено для видимости
    auto panelWidth = (area.getWidth() - centerGap) / 2;

    auto leftPanel = area.removeFromLeft(panelWidth).reduced(4, 0);
    auto linkPanel = area.removeFromLeft(centerGap); // Место для красоты
    auto rightPanel = area.reduced(4, 0);

    // Устанавливаем границы Групп (Рамки)
    satGroup.setBounds(leftPanel);
    neuralLink.setBounds(linkPanel.reduced(0, 20)); // Чуть отступ сверху/снизу
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
    driveSlider.setBounds(driveArea.withSizeKeepingCentre(150, 150)); // Fixed 150px size

    // Transfer Function Display - под Drive ручкой
    auto scopeArea = topHalf.removeFromBottom(70).reduced(10, 5);
    shaperScope.setBounds(scopeArea);

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
        toneFlex.items.add(juce::FlexItem(*k).withFlex(1.0f).withMaxWidth(150).withMaxHeight(150).withMargin(2.0f));
    }

    toneFlex.performLayout(area.reduced(0, 5)); // Немного воздуха сверху/снизу
}

// --- ХЕЛПЕР: Раскладка Сети ---
void CoheraSaturatorAudioProcessorEditor::layoutNetwork(juce::Rectangle<int> area)
{
    // 1. HEADER: Оба селектора в одну строку (режим + краска сатурации)
    auto headerArea = area.removeFromTop(35); // Немного меньше высоты

    // FlexBox для двух селекторов в ряд
    juce::FlexBox headerFlex;
    headerFlex.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;

    // Левый селектор: Interaction Mode
    headerFlex.items.add(juce::FlexItem(netModeSelector).withFlex(1.0f).withMaxHeight(24));

    // Правый селектор: Reaction Type (краска сатурации)
    headerFlex.items.add(juce::FlexItem(netSatSelector).withFlex(1.0f).withMaxHeight(24));

    headerFlex.performLayout(headerArea.reduced(5, 5)); // Небольшой отступ

    // Оставшееся делим: Слева ручки, Справа Метр
    // Метр занимает 15% ширины справа - temporarily disabled
    // auto meterArea = area.removeFromRight(area.getWidth() * 0.15f).reduced(5, 10);
    // interactionMeter.setBounds(meterArea);

    // Ручки: все 3 в один ряд (Sens, Depth, Smooth)
    auto knobArea = area.reduced(5, 0);

    juce::FlexBox netFlex;
    netFlex.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // Равномерное распределение

    // Все три ручки в одном ряду
    netFlex.items.add(juce::FlexItem(netSensSlider).withFlex(1.0f).withMaxWidth(150).withMaxHeight(150));
    netFlex.items.add(juce::FlexItem(netDepthSlider).withFlex(1.0f).withMaxWidth(150).withMaxHeight(150));
    netFlex.items.add(juce::FlexItem(netSmoothSlider).withFlex(1.0f).withMaxWidth(150).withMaxHeight(150));

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
    // 5 ручек в grid: Heat, Drift, Variance, Entropy, Noise
    juce::FlexBox mojoFlex;
    mojoFlex.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // Лучший grid

    juce::Slider* mojoKnobs[] = { &heatSlider, &driftSlider, &varianceSlider, &entropySlider, &noiseSlider };

    // Mojo ручки в grid размещении
    for (auto* k : mojoKnobs) {
        mojoFlex.items.add(juce::FlexItem(*k).withFlex(1.0f).withMaxWidth(150).withMaxHeight(150).withMargin(juce::FlexItem::Margin(0, 2, 0, 2)));
    }
    mojoFlex.performLayout(leftSection.reduced(0, 5));

    // === 2. MIX CENTER ===
    // Mix Knob
    mixSlider.setBounds(centerSection.withSizeKeepingCentre(150, 150)); // 2.5x bigger

    // Delta Button (Маленькая кнопка рядом с Mix)
    int btnSize = 20;
    deltaButton.setBounds(mixSlider.getRight() - 10, mixSlider.getY(), btnSize, btnSize);

    // === 3. OUTPUT SECTION (Right) ===
    // Focus и Output в grid
    juce::FlexBox outFlex;
    outFlex.justifyContent = juce::FlexBox::JustifyContent::spaceAround; // Лучший grid

    outFlex.items.add(juce::FlexItem(focusSlider).withFlex(1.0f).withMaxWidth(150).withMaxHeight(150).withMargin(5));
    outFlex.items.add(juce::FlexItem(outputSlider).withFlex(1.0f).withMaxWidth(150).withMaxHeight(150).withMargin(5));

    outFlex.performLayout(rightSection.reduced(0, 5));
}