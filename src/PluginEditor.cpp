#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
CoheraSaturatorAudioProcessorEditor::CoheraSaturatorAudioProcessorEditor(
    CoheraSaturatorAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p), networkBrain(p.getAPVTS()) {
  // 1. Применяем LookAndFeel
  lookAndFeel = std::make_unique<CoheraUI::CoheraLookAndFeel>();
  setLookAndFeel(lookAndFeel.get());

  // 2. Создаем компоненты
  addAndMakeVisible(shakerContainer);

  // --- LAYER 1: BACKGROUND ---
  shakerContainer.addAndMakeVisible(cosmicDust);
  shakerContainer.addAndMakeVisible(techDecor);
  // HorizonGrid отключен для производительности
  // shakerContainer.addAndMakeVisible(horizonGrid);

  techDecor.toBack();
  cosmicDust.toBack();

  // --- LAYER 2: HUD ---
  // HeadsUpDisplay отключен для производительности
  // shakerContainer.addAndMakeVisible(hud);

  // --- LAYER 3: CONTENT ---

  // --- TOP BAR ---
  shakerContainer.addAndMakeVisible(groupSelector);
  groupSelector.addItemList({"Group 1", "Group 2", "Group 3", "Group 4",
                             "Group 5", "Group 6", "Group 7", "Group 8"},
                            1);
  groupSelector.setSelectedId(1);
  groupAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          p.getAPVTS(), "group_id", groupSelector);

  shakerContainer.addAndMakeVisible(roleSelector);
  roleSelector.addItemList({"Listener", "Reference"}, 1);
  roleSelector.setSelectedId(1);
  roleAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          p.getAPVTS(), "role", roleSelector);

  // Quality Selector
  shakerContainer.addAndMakeVisible(qualitySelector);
  qualitySelector.addItemList({"Eco Mode", "Pro (4x)"}, 1);
  qualityAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          p.getAPVTS(), "quality", qualitySelector);

  // Scale Selector (New)
  /*
  shakerContainer.addAndMakeVisible(scaleSelector);
  scaleSelector.addItemList({"75%", "100%", "125%", "150%", "200%"}, 1);
  scaleSelector.setSelectedId(2); // 100% default
  scaleSelector.onChange = [this] {
    float scales[] = {0.75f, 1.0f, 1.25f, 1.50f, 2.0f};
    int id = scaleSelector.getSelectedId();
    if (id > 0 && id <= 5) {
      currentScale = scales[id - 1];

      // 1. Расширяем лимиты, чтобы новый размер влез
      setResizeLimits(600, 400, 3000, 2000);
      setResizable(true, true);

      int newWidth = juce::roundToInt(900 * currentScale);
      int newHeight = juce::roundToInt(650 * currentScale);

      // 2. Пробуем изменить размер окна напрямую (для Standalone)
      if (auto* topLevel = getTopLevelComponent()) {
          topLevel->setSize(newWidth, newHeight);
      }

      // 3. Меняем размер редактора (для VST/AU)
      setSize(newWidth, newHeight);

      // 4. Фиксируем размер (но оставляем лимиты широкими на всякий случай)
      setResizable(false, false);
    }
  };
  */

  // --- VISOR ---
  shakerContainer.addAndMakeVisible(spectrumVisor);
  // BioScanner - оптимизирован и включен обратно
  shakerContainer.addAndMakeVisible(bioScanner);

  // --- COSMIC NEBULA SHAPER (Transfer Function Overlay) ---
  nebulaShaper = std::make_unique<NebulaShaper>(audioProcessor);
  shakerContainer.addAndMakeVisible(*nebulaShaper);

  // --- ENERGY LINK ---
  // shakerContainer.addAndMakeVisible(energyLink);

  // --- PLASMA CORE (Central Energy Reactor) ---
  shakerContainer.addAndMakeVisible(plasmaCore);

  // --- NETWORK BRAIN ---
  shakerContainer.addAndMakeVisible(networkBrain);

  // --- INTERACTION METER ---
  // shakerContainer.addAndMakeVisible(interactionMeter);

  // --- SATURATION CORE (Left) ---
  shakerContainer.addAndMakeVisible(satGroup);

  // Algorithm Selector
  shakerContainer.addAndMakeVisible(mathModeSelector);
  mathModeSelector.addItemList(
      juce::StringArray{
          // === DIVINE SERIES ===
          "Golden Ratio", "Euler Tube", "Pi Fold", "Fibonacci", "Super Ellipse",
          // === COSMIC PHYSICS ===
          "Lorentz Force", "Riemann Zeta", "Mandelbrot Set", "Quantum Well",
          "Planck Limit",
          // === CLASSIC SERIES ===
          "Analog Tape", "Vintage Console", "Diode Class A", "Tube Driver",
          "Digital Fuzz", "Bit Decimator", "Rectifier"},
      1);
  mathModeSelector.setSelectedId(1);
  mathModeAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          p.getAPVTS(), "math_mode", mathModeSelector);

  // Cascade Button - Output Limiter Stage
  shakerContainer.addAndMakeVisible(cascadeButton);
  cascadeButton.setButtonText("CASCADE");
  cascadeButton.setClickingTogglesState(true);
  cascadeButton.setColour(juce::TextButton::buttonOnColourId,
                          juce::Colours::orange.withAlpha(0.6f));
  cascadeAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          p.getAPVTS(), "cascade", cascadeButton);

  // Drive Big Knob (Reactor Knob with RMS animation)
  shakerContainer.addAndMakeVisible(driveSlider);
  driveSlider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  driveSlider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  driveSlider.setColour(juce::Slider::thumbColourId, CoheraUI::kOrangeNeon);
  driveSlider.setName(
      "DRIVE"); // Для правильного выбора цвета в drawRotarySlider

  // Подключаем RMS источник для анимации реактора
  driveSlider.setRMSGetter(
      [this]() { return audioProcessor.getProcessingEngine().getInputRMS(); });

  driveAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          p.getAPVTS(), "drive_master", driveSlider);

  // Dynamics Attachment
  dynamicsAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          p.getAPVTS(), "dynamics", dynamicsSlider);

  // Output Attachment
  outputAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          p.getAPVTS(), "output_gain", outputSlider);

  // Focus Attachment
  focusAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          p.getAPVTS(), "focus", focusSlider);

  // Mojo Attachments
  heatAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          p.getAPVTS(), "heat", heatSlider);
  driftAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          p.getAPVTS(), "analog_drift", driftSlider);
  varianceAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          p.getAPVTS(), "variance", varianceSlider);
  entropyAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          p.getAPVTS(), "entropy", entropySlider);
  noiseAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          p.getAPVTS(), "noise", noiseSlider);

  // Tone Knobs
  setupKnob(tightenSlider, "tone_tighten", "TIGHTEN", CoheraUI::kOrangeNeon);
  setupKnob(punchSlider, "punch", "PUNCH", CoheraUI::kOrangeNeon);
  setupKnob(smoothSlider, "tone_smooth", "SMOOTH", CoheraUI::kOrangeNeon);

  // --- NETWORK BRAIN (Right) ---
  // shakerContainer.addAndMakeVisible(netGroup);

  // Network Mode Selector
  // shakerContainer.addAndMakeVisible(netModeSelector);
  netModeSelector.addItemList(
      juce::StringArray{"Unmasking (Duck)", "Ghost (Follow)", "Gated (Reverse)",
                        "Stereo Bloom", "Sympathetic"},
      1);
  netModeSelector.setSelectedId(1);
  // netModeAttachment temporarily disabled

  // Net Saturation Selector
  // shakerContainer.addAndMakeVisible(netSatSelector);
  netSatSelector.addItemList(
      {"Clean Gain", "Drive Boost", "Rectify", "Bit Crush"}, 1);
  // netSatAttachment temporarily disabled

  // Network Knobs
  // setupKnob(netSensSlider, "net_sens", "SENS", CoheraUI::kCyanNeon);
  // setupKnob(netDepthSlider, "net_depth", "DEPTH", CoheraUI::kCyanNeon);
  // setupKnob(netSmoothSlider, "net_smooth", "RELEASE", CoheraUI::kCyanNeon);

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
  // networkBrain.setAPVTS(p.getAPVTS());
  // addAndMakeVisible(interactionMeter);

  // --- BOTTOM MIX ---
  setupKnob(mixSlider, "mix", "MIX", CoheraUI::kTextBright);

  // Delta Button
  shakerContainer.addAndMakeVisible(deltaButton);
  deltaButton.setButtonText(u8"Δ");
  deltaButton.setClickingTogglesState(true);
  deltaButton.setColour(juce::TextButton::buttonOnColourId,
                        juce::Colours::yellow.withAlpha(0.6f));
  deltaAttachment =
      std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
          p.getAPVTS(), "delta", deltaButton);

  // === NEW VISUAL SYSTEM v2.0 ===
  // FIXED: Components now start timers only after being added to tree
  // CosmicDust полностью отключен для производительности
  // shakerContainer.addAndMakeVisible(cosmicDust);
  // cosmicDust.toBack();

  shakerContainer.addAndMakeVisible(neuralLink);
  neuralLink.setAPVTS(audioProcessor.getAPVTS());

  // --- LAYER 4: OVERLAY ---
  shakerContainer.addAndMakeVisible(textureOverlay);
  textureOverlay.setInterceptsMouseClicks(false, false);

  shakerContainer.addAndMakeVisible(glitchOverlay);
  glitchOverlay.toFront(true);

  // Запускаем таймер редактора, чтобы PlasmaCore/визуализации обновлялись
  startTimerHz(30);

  // Базовый размер
  // currentScale = 1.0f; // Initialize currentScale
  setSize(900, 650);
  setResizable(false, false); // Disable resize, use scale selector
}

CoheraSaturatorAudioProcessorEditor::~CoheraSaturatorAudioProcessorEditor() {
  setLookAndFeel(nullptr);
  lookAndFeel.reset();
}

// Хелпер для быстрой настройки ручек
void CoheraSaturatorAudioProcessorEditor::setupKnob(juce::Slider &s,
                                                    juce::String paramId,
                                                    juce::String displayName,
                                                    juce::Colour c) {
  shakerContainer.addAndMakeVisible(s);
  s.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
  s.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
  s.setColour(juce::Slider::thumbColourId, c);

  // ВАЖНО: Устанавливаем имя для отображения
  s.setName(displayName);

  // Храним аттачменты в векторе, чтобы не создавать кучу named variables
  sliderAttachments.push_back(
      std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
          audioProcessor.getAPVTS(), paramId, s));
}

void CoheraSaturatorAudioProcessorEditor::paint(juce::Graphics &g) {
  // Фон уже рисуется компонентами (CosmicDust, TechDecor)
  // Но на всякий случай зальем черным, чтобы не было артефактов при ресайзе
  g.fillAll(CoheraUI::kBackground);
}

void CoheraSaturatorAudioProcessorEditor::paintOverChildren(juce::Graphics &g) {
  float flashAlpha = screenShaker.getFlashAlpha();

  if (flashAlpha > 0.01f) {
    juce::Colour flashColor = CoheraUI::kOrangeNeon.withAlpha(flashAlpha);

    auto center = getLocalBounds().getCentre().toFloat();
    juce::ColourGradient flash(juce::Colours::transparentWhite, center.x,
                               center.y, flashColor, 0.0f, getWidth() * 0.7f,
                               true);

    g.setGradientFill(flash);
    g.fillAll();
  }

  // Логотип с легким свечением
  g.setColour(CoheraUI::kTextBright);
  // FUTURA BOLD + KERNING
  g.setFont(juce::Font("Futura", 24.0f, juce::Font::bold)
                .withExtraKerningFactor(0.2f));
  g.drawText("COHERA", 20, 0, 200, 50, juce::Justification::centredLeft);

  // Вторая часть логотипа другим весом шрифта
  g.setColour(CoheraUI::kOrangeNeon);
  // FUTURA PLAIN + KERNING
  g.setFont(juce::Font("Futura", 24.0f, juce::Font::plain)
                .withExtraKerningFactor(0.2f));
  // Сдвигаем правее (140px), так как шрифт шире из-за кернинга
  g.drawText("SATURATOR", 140, 0, 200, 50, juce::Justification::centredLeft);
}

// Timer callback для обновления живых визуализаторов
void CoheraSaturatorAudioProcessorEditor::timerCallback() {
  // Безопасность: проверяем видимость
  if (!isVisible())
    return;

  // === TRAUMA SYSTEM (Screen Shake) ===
  float transientLevel = audioProcessor.getTransientLevel(); // 0..1

  // Триггер удара: Если удар сильный, добавляем травму
  if (transientLevel > 0.6f) {
    float impact = (transientLevel - 0.6f) * 1.5f; // Scale impact
    screenShaker.addImpact(impact);
  }

  screenShaker.update();

  // Применяем тряску
  auto offset = screenShaker.getShakeOffset(12.0f); // 12px max shake
  shakerContainer.setTransform(
      juce::AffineTransform::translation(offset.x, offset.y));

  // Если есть травма - перерисовываем весь эдитор для эффекта вспышки
  if (screenShaker.getFlashAlpha() > 0.01f) {
    repaint();
  }

  // Обновляем энергию для визуализаторов
  float inputRMS = audioProcessor.getInputRMS();
  float outputRMS = audioProcessor.getOutputRMS();

  // CosmicDust и HorizonGrid отключены для производительности
  // cosmicDust.setEnergyLevel(outputRMS);
  // horizonGrid.setEnergyLevel(outputRMS);
  // HeadsUpDisplay отключен для производительности
  // hud.setEnergyLevel(outputRMS);
  neuralLink.setEnergyLevel(inputRMS);
  glitchOverlay.setEnergyLevel(transientLevel);
  // BioScanner - оптимизирован и включен обратно
  bioScanner.setEnergyLevel(outputRMS);

  // Обрабатываем FFT данные
  audioProcessor.processFFTForGUI();

  // Обновляем FFT данные для SpectrumVisor
  spectrumVisor.setFFTData(audioProcessor.getFFTData());

  // TransferFunctionDisplay - безопасный доступ к параметрам
  auto &apvts = audioProcessor.getAPVTS();
  if (auto *driveParam = apvts.getRawParameterValue("drive_master")) {
    float drive = *driveParam;

    Cohera::SaturationMode mathMode = Cohera::SaturationMode::GoldenRatio;
    if (auto *mathModeParam = apvts.getRawParameterValue("math_mode")) {
      mathMode = static_cast<Cohera::SaturationMode>((int)*mathModeParam);
    }

    bool cascade = false;
    if (auto *cascadeParam = apvts.getRawParameterValue("cascade")) {
      cascade = *cascadeParam > 0.5f;
    }
  }

  // Собираем данные для Плазмы
  PlasmaState plasmaState;

  // 1. Искажение от Драйва (положение ручки + RMS)
  float drivePos = driveSlider.getValue() / driveSlider.getMaximum();
  plasmaState.driveLevel = drivePos * audioProcessor.getInputRMS();

  // 2. Каналы (для красоты берем RMS и немного разносим, если есть Variance)
  // Можно взять реальные L/R RMS из процессора, если добавить геттеры
  // Пока используем общий RMS для симметрии, но с модуляцией от Variance
  float var = 0.0f;
  if (auto *varianceParam = apvts.getRawParameterValue("variance")) {
    var = *varianceParam / 100.0f;
  }
  plasmaState.leftSignal = audioProcessor.getInputRMS() * (1.0f - var * 0.2f);
  plasmaState.rightSignal = audioProcessor.getInputRMS() * (1.0f + var * 0.2f);

  // 3. Сеть (берем из параметра net_sens)
  if (auto *netSensParam = apvts.getRawParameterValue("net_sens")) {
    plasmaState.netModulation =
        *netSensParam / 100.0f; // Нормализуем от 0..100 к 0..1
  } else {
    plasmaState.netModulation = 0.0f;
  }

  // 4. Глобальная перегрузка (Heat)
  // Берем из атомика Depth (так как Depth показывает общую модуляцию)
  // Или добавляем спец. детектор перегруза.
  // Используем outputRMS > 0.9 как "Heat Flash"
  float outPeak = audioProcessor.getOutputRMS();
  plasmaState.globalHeat = (outPeak > 0.8f) ? (outPeak - 0.8f) * 5.0f
                                            : 0.0f; // Вспышка только на пиках

  // Отправляем в ядро
  plasmaCore.updateState(plasmaState);
}

void CoheraSaturatorAudioProcessorEditor::resized() {
  // ВАЖНО: При изменении размера окна мы НЕ меняем верстку.
  // Мы просто обновляем масштаб контейнера.
  // Верстка всегда происходит в координатах 900x650.

  // 1. Устанавливаем bounds контейнера в БАЗОВЫЙ размер
  shakerContainer.setBounds(0, 0, 900, 650);

  // Применяем трансформ
  // shakerContainer.setTransform(juce::AffineTransform::scale(currentScale));
  shakerContainer.setTransform(juce::AffineTransform::identity);

  techDecor.setBounds(shakerContainer.getLocalBounds());
  textureOverlay.setBounds(shakerContainer.getLocalBounds());
  glitchOverlay.setBounds(shakerContainer.getLocalBounds());
  // CosmicDust отключен для производительности
  // cosmicDust.setBounds(bounds);
  // HorizonGrid отключен для производительности
  // horizonGrid.setBounds(bounds);
  // HeadsUpDisplay отключен для производительности
  // hud.setBounds(bounds);
  textureOverlay.generateTexture(900, 650); // Generate for base size

  // 1. Контейнер занимает ВЕСЬ экран.
  // Мы будем двигать его Transform, а не Bounds.
  // 2. Вся верстка теперь происходит относительно shakerContainer!
  auto area = shakerContainer.getLocalBounds();

  // 1. Глобальные отступы (Padding)
  // "Воздух" по краям — признак дорогого дизайна
  area.reduce(16, 16);

  // ==============================================================================
  // 🔝 HEADER & VISOR (35% высоты)
  // ==============================================================================
  auto topSection = area.removeFromTop(
      static_cast<int>(650 * 0.38f)); // Use fixed height base

  // Top Bar (Selectors) - 40px height fixed inside proportional area
  auto topBar = topSection.removeFromTop(40);

  // Right Align selectors: Scale -> Role -> Group -> Quality
  // scaleSelector.setBounds(topBar.removeFromRight(70)); // Scale
  topBar.removeFromRight(10); // Spacer
  roleSelector.setBounds(topBar.removeFromRight(100));
  topBar.removeFromRight(10); // Spacer
  groupSelector.setBounds(topBar.removeFromRight(80));
  topBar.removeFromRight(10); // Spacer
  qualitySelector.setBounds(topBar.removeFromRight(90));

  topSection.removeFromTop(10); // Spacer to Visor

  // Visor занимает всё оставшееся место в топе
  spectrumVisor.setBounds(topSection);
  // BioScanner - оптимизирован и включен обратно
  bioScanner.setBounds(spectrumVisor.getBounds());

  // Cosmic Nebula Shaper - Transfer Function Overlay
  if (nebulaShaper)
    nebulaShaper->setBounds(topSection);

  area.removeFromTop(16); // Spacer между Визором и Панелями

  // ==============================================================================
  // 🦶 FOOTER (18% высоты) - Снизу вверх
  // ==============================================================================
  auto footerHeight =
      static_cast<int>(650 * 0.20f); // Увеличено для больших Mojo ручек
  auto footerArea = area.removeFromBottom(footerHeight);
  layoutFooter(footerArea);

  area.removeFromBottom(16); // Spacer перед футером

  // ==============================================================================
  // 🎛️ MAIN PANELS (Оставшееся место по центру)
  // ==============================================================================

  // Делим на 3 части: Left Panel | Link (Gap) | Right Panel
  auto centerGap =
      area.getWidth() * 0.12f; // 12% ширины на связку - увеличено для видимости
  auto panelWidth = (area.getWidth() - centerGap) / 2;

  auto leftPanel = area.removeFromLeft(panelWidth).reduced(4, 0);
  auto linkPanel = area.removeFromLeft(centerGap); // Место для красоты
  auto rightPanel = area.reduced(4, 0);

  // Устанавливаем границы Групп (Рамки)
  satGroup.setBounds(leftPanel);
  plasmaCore.setBounds(linkPanel.reduced(0, 10)); // Чуть отступаем сверху/снизу
  networkBrain.setBounds(rightPanel);

  // Заполняем внутренности групп (с учетом отступа под заголовок группы)
  // Отступ сверху 30px под текст "SATURATION CORE"
  layoutSaturation(leftPanel.reduced(12, 12).withTrimmedTop(25));
  // layoutNetwork(rightPanel.reduced(12, 12).withTrimmedTop(25));
}

// --- ХЕЛПЕР: Раскладка Сатурации ---
void CoheraSaturatorAudioProcessorEditor::layoutSaturation(
    juce::Rectangle<int> area) {
  // Верхняя половина: Drive (King) + Control Bar (Algo + Cascade)
  auto topHalf = area.removeFromTop(area.getHeight() * 0.55f);

  // Drive Knob - Главный герой, по центру левой части
  // Занимает 55% ширины (чуть меньше, чтобы влезли селекторы)
  auto driveArea = topHalf.removeFromLeft(topHalf.getWidth() * 0.55f);
  driveSlider.setBounds(
      driveArea.withSizeKeepingCentre(150, 150)); // Fixed 150px size

  // Transfer Function Display теперь поверх анализатора (в layoutMainPanels)

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
  cascadeButton.setBounds(
      controlRect.removeFromTop(controlHeight)); // Cascade Button

  // Нижняя половина: 4 ручки тона в ряд (Tighten, Punch, Dyn, Smooth)
  // Используем FlexBox для идеального распределения
  juce::FlexBox toneFlex;
  toneFlex.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;

  // Массив ручек для добавления
  juce::Slider *knobs[] = {&tightenSlider, &punchSlider, &dynamicsSlider,
                           &smoothSlider};

  for (auto *k : knobs) {
    toneFlex.items.add(juce::FlexItem(*k)
                           .withFlex(1.0f)
                           .withMaxWidth(150)
                           .withMaxHeight(150)
                           .withMargin(2.0f));
  }

  toneFlex.performLayout(area.reduced(0, 5)); // Немного воздуха сверху/снизу
}

// --- ХЕЛПЕР: Раскладка Сети ---
void CoheraSaturatorAudioProcessorEditor::layoutNetwork(
    juce::Rectangle<int> area) {
  // 1. HEADER: Оба селектора в одну строку (режим + краска сатурации)
  auto headerArea = area.removeFromTop(35); // Немного меньше высоты

  // FlexBox для двух селекторов в ряд
  juce::FlexBox headerFlex;
  headerFlex.justifyContent = juce::FlexBox::JustifyContent::spaceBetween;

  // Левый селектор: Interaction Mode
  headerFlex.items.add(
      juce::FlexItem(netModeSelector).withFlex(1.0f).withMaxHeight(24));

  // Правый селектор: Reaction Type (краска сатурации)
  headerFlex.items.add(
      juce::FlexItem(netSatSelector).withFlex(1.0f).withMaxHeight(24));

  headerFlex.performLayout(headerArea.reduced(5, 5)); // Небольшой отступ

  // Meter (справа)
  auto meterArea = area.removeFromRight(area.getWidth() * 0.15f).reduced(5, 10);
  // interactionMeter.setBounds(meterArea);

  // Ручки: все 3 в один ряд (Sens, Depth, Smooth)
  auto knobArea = area.reduced(5, 0);

  juce::FlexBox netFlex;
  netFlex.justifyContent =
      juce::FlexBox::JustifyContent::spaceAround; // Равномерное распределение

  // Все три ручки в одном ряду (temporarily disabled - sliders not declared)
  // netFlex.items.add(juce::FlexItem(netSensSlider)
  //                     .withFlex(1.0f)
  //                     .withMaxWidth(150)
  //                     .withMaxHeight(150));
  // netFlex.items.add(juce::FlexItem(netDepthSlider)
  //                     .withFlex(1.0f)
  //                     .withMaxWidth(150)
  //                     .withMaxHeight(150));
  // netFlex.items.add(juce::FlexItem(netSmoothSlider)
  //                     .withFlex(1.0f)
  //                     .withMaxWidth(150)
  //                     .withMaxHeight(150));

  netFlex.performLayout(knobArea);
}

// --- ХЕЛПЕР: Футер (Mix & Mojo) ---
void CoheraSaturatorAudioProcessorEditor::layoutFooter(
    juce::Rectangle<int> area) {
  // Простое разделение на 3 равные части
  int sectionWidth = area.getWidth() / 3;

  auto leftSection = area.removeFromLeft(sectionWidth);
  auto centerSection = area.removeFromLeft(sectionWidth);
  auto rightSection = area; // Оставшееся

  // === 1. MOJO RACK (Left) ===
  // 5 ручек в grid: Heat, Drift, Variance, Entropy, Noise
  juce::FlexBox mojoFlex;
  mojoFlex.justifyContent =
      juce::FlexBox::JustifyContent::spaceAround; // Лучший grid

  juce::Slider *mojoKnobs[] = {&heatSlider, &driftSlider, &varianceSlider,
                               &entropySlider, &noiseSlider};

  // Mojo ручки в grid размещении
  for (auto *k : mojoKnobs) {
    mojoFlex.items.add(juce::FlexItem(*k)
                           .withFlex(1.0f)
                           .withMaxWidth(150)
                           .withMaxHeight(150)
                           .withMargin(juce::FlexItem::Margin(0, 2, 0, 2)));
  }
  mojoFlex.performLayout(leftSection.reduced(0, 5));

  // === 2. MIX CENTER ===
  // Mix Knob
  mixSlider.setBounds(
      centerSection.withSizeKeepingCentre(150, 150)); // 2.5x bigger

  // Delta Button (Маленькая кнопка рядом с Mix)
  int btnSize = 20;
  deltaButton.setBounds(mixSlider.getRight() - 10, mixSlider.getY(), btnSize,
                        btnSize);

  // === 3. OUTPUT SECTION (Right) ===
  // Focus и Output в grid
  juce::FlexBox outFlex;
  outFlex.justifyContent =
      juce::FlexBox::JustifyContent::spaceAround; // Лучший grid

  outFlex.items.add(juce::FlexItem(focusSlider)
                        .withFlex(1.0f)
                        .withMaxWidth(150)
                        .withMaxHeight(150)
                        .withMargin(5));
  outFlex.items.add(juce::FlexItem(outputSlider)
                        .withFlex(1.0f)
                        .withMaxWidth(150)
                        .withMaxHeight(150)
                        .withMargin(5));

  outFlex.performLayout(rightSection.reduced(0, 5));
}