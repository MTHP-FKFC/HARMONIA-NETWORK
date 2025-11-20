# 🏗️ ПЛАН РЕФАКТОРИНГА COHERA SATURATOR
## Принципы ООП и Clean Architecture

---

## 📊 ТЕКУЩИЕ ПРОБЛЕМЫ

### 1. **God Object Anti-Pattern**
- `PluginProcessor` делает слишком много:
  - Управление параметрами (APVTS)
  - DSP обработка (все модули)
  - Сетевая коммуникация
  - Oversampling
  - Буферы и память
  - Инициализация всех модулей

### 2. **Нарушение Single Responsibility Principle (SRP)**
- `processBlock()` содержит ~400 строк и делает:
  - Чтение параметров
  - Сетевое взаимодействие
  - Oversampling
  - Фильтрацию
  - Разделение на полосы
  - Обработку каждой полосы
  - Суммирование
  - Микс Dry/Wet
  - Финальную обработку

### 3. **Отсутствие инкапсуляции**
- Все модули хранятся как массивы в Processor
- Нет абстракций для групп модулей
- Прямой доступ к внутренним структурам

### 4. **Дублирование кода**
- Повторяющаяся логика для L/R каналов
- Одинаковая обработка для всех полос
- Похожие паттерны инициализации

### 5. **Тесная связанность**
- Processor знает детали всех модулей
- Нет интерфейсов/абстракций
- Сложно тестировать отдельные компоненты

---

## 🎯 ЦЕЛИ РЕФАКТОРИНГА

1. **Разделение ответственностей** (SRP)
2. **Инкапсуляция** логики обработки
3. **Композиция** вместо наследования
4. **Абстракции** для модулей
5. **Тестируемость** компонентов
6. **Расширяемость** архитектуры

---

## 🏛️ НОВАЯ АРХИТЕКТУРА

### **Слои (Layers)**

```
┌─────────────────────────────────────┐
│   Presentation Layer                │
│   (PluginProcessor, PluginEditor)   │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│   Business Logic Layer              │
│   (ProcessingEngine, BandEngine)   │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│   DSP Module Layer                 │
│   (MathSaturator, DCBlocker, etc)  │
└─────────────────────────────────────┘
              ↓
┌─────────────────────────────────────┐
│   Infrastructure Layer              │
│   (NetworkManager, Buffers)         │
└─────────────────────────────────────┘
```

---

## 📦 НОВЫЕ КЛАССЫ И СТРУКТУРЫ

### **1. Core Processing Engine**

#### `ProcessingEngine` (Главный движок обработки)
**Ответственность:** Координация всего процесса обработки

**Состав:**
- `OversamplingEngine` - управление oversampling
- `FilterBankEngine` - управление мультибандом
- `BandProcessingEngine[]` - обработка каждой полосы
- `MixEngine` - Dry/Wet микс
- `PostProcessingEngine` - финальная обработка

**Методы:**
- `prepare(sampleRate, blockSize)`
- `processBlock(buffer)`
- `reset()`

---

#### `OversamplingEngine`
**Ответственность:** Управление oversampling и качеством

**Состав:**
- `juce::dsp::Oversampling<float>` oversampler
- `QualityMode` mode (Eco/Pro)

**Методы:**
- `processUp(buffer)` → upsampled buffer
- `processDown(buffer)` → downsampled buffer
- `setQuality(mode)`
- `getLatencySamples()`

---

#### `FilterBankEngine`
**Ответственность:** Управление мультибандным разделением

**Состав:**
- `PlaybackFilterBank` filterBank
- `std::array<AudioBuffer, 6>` bandBuffers
- `EmphasisFilters` preFilters, postFilters

**Методы:**
- `splitIntoBands(inputBuffer)` → bandBuffers
- `sumBands(bandBuffers)` → outputBuffer
- `applyPreFilters(buffer)`
- `applyPostFilters(buffer)`

---

#### `BandProcessingEngine` (Один на полосу)
**Ответственность:** Обработка одной частотной полосы

**Состав:**
- `SaturationEngine` saturationEngine
- `TransientEngine` transientEngine
- `AnalogModelingEngine` analogEngine
- `NetworkModulationEngine` networkEngine
- `DCBlocker` dcBlocker

**Методы:**
- `processBand(input, parameters)` → output
- `prepare(sampleRate)`
- `reset()`

---

#### `SaturationEngine`
**Ответственность:** Применение Divine Math сатурации

**Состав:**
- `MathSaturator` mathSaturator
- `MathMode` currentMode
- `DriveCalculator` driveCalc

**Методы:**
- `processSample(input, drive, mode)` → output
- `setMode(mode)`
- `setDrive(drive)`

---

#### `TransientEngine` (Split & Crush)
**Ответственность:** Разделение и обработка транзиентов

**Состав:**
- `TransientSplitter` splitter
- `SaturationEngine` bodySaturation
- `SaturationEngine` transientSaturation
- `PunchMode` punchMode

**Методы:**
- `processSample(input, punchParam)` → output
- `setPunchMode(mode)` (Clean/Dirty/Neutral)

---

#### `AnalogModelingEngine`
**Ответственность:** Аналоговое моделирование

**Состав:**
- `VoltageRegulator` psu
- `ThermalModel` thermalModel
- `HarmonicEntropy` entropy
- `StereoVariance` variance

**Методы:**
- `processSample(input, globalHeat)` → output
- `applyVoltageStarvation(input)`
- `applyThermalBias(input)`
- `applyEntropy(input)`

---

#### `NetworkModulationEngine`
**Ответственность:** Сетевая модуляция и взаимодействие

**Состав:**
- `NetworkManager&` networkManager
- `EnvelopeFollower` envelope
- `NetworkControls` controls (Depth, Smooth, Sensitivity)
- `NetworkMode` mode (Unmasking, Ghost, Gated, etc)

**Методы:**
- `processAsReference(input)` → sends to network
- `processAsListener(input)` → receives from network
- `getModulationSignal()` → 0.0..1.0
- `setMode(mode)`
- `setControls(depth, smooth, sensitivity)`

---

#### `MixEngine`
**Ответственность:** Dry/Wet микс и финальная обработка

**Состав:**
- `DelayLine` dryDelayLine
- `DCBlocker` masterDCBlocker
- `PsychoAcousticGain` psychoGain
- `DeltaMonitor` deltaMonitor

**Методы:**
- `processMix(dryBuffer, wetBuffer, mixParam)` → output
- `applyPostProcessing(buffer)`
- `applyDeltaMonitoring(buffer)`

---

### **2. Parameter Management**

#### `ParameterManager`
**Ответственность:** Централизованное управление параметрами

**Состав:**
- `juce::AudioProcessorValueTreeState` apvts
- `ParameterCache` cache (для оптимизации)

**Методы:**
- `getDrive()` → float
- `getMix()` → float
- `getMathMode()` → MathMode
- `getQualityMode()` → QualityMode
- `getNetworkMode()` → NetworkMode
- `getAllParameters()` → ParameterSet

---

#### `ParameterSet` (Struct)
**Ответственность:** Группировка параметров для передачи

**Состав:**
- Все параметры в одной структуре
- Сглаженные значения

**Использование:** Передача в Engine'ы без прямого доступа к APVTS

---

### **3. Network Layer**

#### `NetworkController`
**Ответственность:** Управление сетевым взаимодействием

**Состав:**
- `NetworkManager&` networkManager
- `NetworkRole` role (Reference/Listener)
- `int` groupId
- `GlobalHeatController` heatController

**Методы:**
- `sendSignal(bandIndex, value)`
- `receiveSignal(bandIndex)` → float
- `updateGlobalHeat(instanceEnergy)`
- `getGlobalHeat()` → float

---

#### `GlobalHeatController`
**Ответственность:** Управление Global Heat системой

**Состав:**
- `int` instanceIndex
- `LinearSmoothedValue` smoothedHeat

**Методы:**
- `registerInstance()` → instanceIndex
- `updateEnergy(energy)`
- `getGlobalHeat()` → float
- `unregisterInstance()`

---

### **4. State Management**

#### `ProcessingState`
**Ответственность:** Хранение состояния обработки

**Состав:**
- `GainReductionMeter` gainReduction
- `QualityMode` qualityMode
- `bool` isInitialized

**Методы:**
- `updateGainReduction(band, value)`
- `getGainReduction(band)` → float
- `setQualityMode(mode)`

---

## 🔄 ПОСЛЕДОВАТЕЛЬНОСТЬ РЕФАКТОРИНГА

### **Фаза 1: Извлечение Engines (Low Risk)**

1. ✅ Создать `SaturationEngine`
   - Вынести логику сатурации из processBlock
   - Протестировать изолированно

2. ✅ Создать `TransientEngine`
   - Вынести Split & Crush логику
   - Интегрировать с SaturationEngine

3. ✅ Создать `AnalogModelingEngine`
   - Вынести Voltage, Thermal, Entropy, Variance
   - Объединить в один модуль

4. ✅ Создать `NetworkModulationEngine`
   - Вынести сетевую логику
   - Инкапсулировать Reference/Listener

---

### **Фаза 2: Создание Band Engine (Medium Risk)**

5. ✅ Создать `BandProcessingEngine`
   - Объединить все модули обработки полосы
   - Один BandEngine на полосу

6. ✅ Создать `FilterBankEngine`
   - Вынести логику разделения/суммирования
   - Управление Emphasis Filters

7. ✅ Создать `OversamplingEngine`
   - Инкапсулировать oversampling
   - Управление Quality Mode

---

### **Фаза 3: Главный Processing Engine (High Risk)**

8. ✅ Создать `ProcessingEngine`
   - Координировать все Engines
   - Упростить processBlock до вызова engine.processBlock()

9. ✅ Создать `MixEngine`
   - Вынести Dry/Wet логику
   - Post-processing

10. ✅ Рефакторинг `PluginProcessor`
    - Оставить только координацию
    - Делегировать всю обработку в ProcessingEngine

---

### **Фаза 4: Parameter Management (Low Risk)**

11. ✅ Создать `ParameterManager`
    - Централизовать доступ к параметрам
    - Создать ParameterSet для передачи

12. ✅ Создать `ProcessingState`
    - Хранить состояние (Gain Reduction, Quality)
    - Управление инициализацией

---

### **Фаза 5: Network Layer (Medium Risk)**

13. ✅ Создать `NetworkController`
    - Инкапсулировать NetworkManager доступ
    - Упростить Reference/Listener логику

14. ✅ Создать `GlobalHeatController`
    - Вынести Global Heat логику
    - Управление instance registration

---

### **Фаза 6: Финальная интеграция (High Risk)**

15. ✅ Интегрировать все Engines в ProcessingEngine
16. ✅ Упростить PluginProcessor до минимума
17. ✅ Тестирование всей системы
18. ✅ Оптимизация производительности

---

## 📋 ПРИНЦИПЫ РЕАЛИЗАЦИИ

### **1. Single Responsibility Principle (SRP)**
- Каждый класс отвечает за одну вещь
- SaturationEngine → только сатурация
- TransientEngine → только транзиенты
- NetworkEngine → только сеть

### **2. Open/Closed Principle (OCP)**
- Классы открыты для расширения, закрыты для модификации
- Использовать интерфейсы для модулей
- Позволить добавлять новые алгоритмы без изменения кода

### **3. Dependency Inversion Principle (DIP)**
- Зависимости от абстракций, не от конкретных классов
- Engines зависят от интерфейсов модулей
- Легко заменить реализацию

### **4. Composition over Inheritance**
- Использовать композицию для объединения модулей
- BandEngine содержит SaturationEngine, TransientEngine и т.д.
- Нет глубоких иерархий наследования

### **5. Interface Segregation Principle (ISP)**
- Интерфейсы должны быть маленькими и специфичными
- Не заставлять классы реализовывать ненужные методы

---

## 🎨 СТРУКТУРА ФАЙЛОВ

```
src/
├── engine/
│   ├── ProcessingEngine.h/cpp
│   ├── OversamplingEngine.h/cpp
│   ├── FilterBankEngine.h/cpp
│   ├── BandProcessingEngine.h/cpp
│   ├── SaturationEngine.h/cpp
│   ├── TransientEngine.h/cpp
│   ├── AnalogModelingEngine.h/cpp
│   ├── NetworkModulationEngine.h/cpp
│   └── MixEngine.h/cpp
├── parameters/
│   ├── ParameterManager.h/cpp
│   └── ParameterSet.h
├── network/
│   ├── NetworkController.h/cpp
│   └── GlobalHeatController.h/cpp
├── state/
│   └── ProcessingState.h/cpp
└── dsp/
    └── (существующие модули остаются)
```

---

## ✅ КРИТЕРИИ УСПЕХА

1. **PluginProcessor.processBlock()** < 50 строк
2. **Каждый Engine** < 200 строк
3. **Тестируемость** - можно тестировать каждый Engine отдельно
4. **Расширяемость** - легко добавить новый алгоритм
5. **Производительность** - нет деградации после рефакторинга
6. **Читаемость** - код понятен без комментариев

---

## 🚨 РИСКИ И МИТИГАЦИЯ

### **Риск 1: Потеря производительности**
- **Митигация:** Профилирование до/после
- **Проверка:** Benchmark тесты

### **Риск 2: Регрессии в функциональности**
- **Митигация:** Пошаговый рефакторинг
- **Проверка:** Unit тесты для каждого Engine

### **Риск 3: Сложность интеграции**
- **Митигация:** Начать с простых модулей
- **Проверка:** Инкрементальная интеграция

---

## 📝 СЛЕДУЮЩИЕ ШАГИ

1. **Обсудить план** - убедиться что все понятно
2. **Выбрать фазу** - с чего начать рефакторинг
3. **Создать первый Engine** - как proof of concept
4. **Тестировать** - убедиться что работает
5. **Продолжить** - пошагово рефакторить остальное

---

**Готов начать рефакторинг! С какой фазы начнем?** 🚀

