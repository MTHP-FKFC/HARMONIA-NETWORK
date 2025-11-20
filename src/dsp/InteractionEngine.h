#pragma once

#include "Waveshaper.h"

struct DualShaperConfig
{
    SaturationType typeA; // Состояние покоя
    float driveScaleA;    // Множитель драйва А

    SaturationType typeB; // Состояние удара (Сайдчейн)
    float driveScaleB;    // Множитель драйва B (обычно буст)
};

class InteractionEngine
{
public:
    // Метод возвращает конфигурацию для конкретной полосы и режима
    static DualShaperConfig getConfiguration(int modeIndex, int bandIndex, SaturationType userSelectedType)
    {
        // 0 = Unmasking, 1 = Ghost, 2 = Gated, 3 = Bloom, 4 = Sympathetic

        // По дефолту: A = Выбор юзера, B = Выбор юзера (нет изменений)
        DualShaperConfig cfg { userSelectedType, 1.0f, userSelectedType, 1.0f };

        switch (modeIndex)
        {
            case 0: // UNMASKING (Ducking)
                // Когда бьет бочка (B), мы становимся Clean и тише (0.5 drive)
                cfg.typeA = userSelectedType;
                cfg.typeB = SaturationType::Clean;
                cfg.driveScaleB = 0.2f; // Сильно уменьшаем драйв
                break;

            case 1: // GHOST (Harmonics)
                // Только для НЧ полос (0 и 1) меняем тип на Rectifier
                if (bandIndex <= 1) {
                    cfg.typeA = userSelectedType;
                    cfg.typeB = SaturationType::Rectifier;
                    cfg.driveScaleB = 2.0f; // Бустим драйв для яркости эффекта
                }
                // Для ВЧ можно сделать BitCrush для агрессии
                else if (bandIndex >= 4) {
                    cfg.typeA = userSelectedType;
                    cfg.typeB = SaturationType::BitCrush;
                    cfg.driveScaleB = 1.5f;
                }
                break;

            case 2: // GATED (Reverse)
                // В покое (A) мы грязные (Rectifier/Crush).
                // При ударе (B) становимся чистыми (UserType).
                if (bandIndex <= 2) {
                    cfg.typeA = SaturationType::Rectifier; // Грязь в паузах
                    cfg.driveScaleA = 1.5f;
                    cfg.typeB = userSelectedType;          // Чистота на ударе
                }
                break;

            case 3: // STEREO BLOOM
                // (Логика M/S обрабатывается снаружи, здесь типы одинаковые)
                // Просто бустим драйв на ударе
                cfg.driveScaleB = 1.5f;
                break;

            case 4: // SYMPATHETIC
                // Добавляем гармоники (Asymmetric) на резонансе
                cfg.typeB = SaturationType::Asymmetric;
                cfg.driveScaleB = 1.3f;
                break;
        }

        return cfg;
    }

    // Процессинг с морфингом
    static float processMorph(float input, float baseDrive, float morph, const DualShaperConfig& cfg)
    {
        // Рассчитываем два варианта
        // Если morph = 0 (нет сигнала сети), слышим только A.
        // Если morph = 1 (пик сигнала), слышим только B.

        // Оптимизация: если morph близко к краям, не считаем второй вариант
        if (morph < 0.01f) {
            return Waveshaper::process(input, baseDrive * cfg.driveScaleA, cfg.typeA);
        }
        if (morph > 0.99f) {
            return Waveshaper::process(input, baseDrive * cfg.driveScaleB, cfg.typeB);
        }

        float outA = Waveshaper::process(input, baseDrive * cfg.driveScaleA, cfg.typeA);
        float outB = Waveshaper::process(input, baseDrive * cfg.driveScaleB, cfg.typeB);

        // Линейный кроссфейд
        return outA * (1.0f - morph) + outB * morph;
    }
};
