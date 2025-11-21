#pragma once
#include "../../JuceHeader.h"
#include <cmath>

class ScreenShaker
{
public:
    ScreenShaker() {}

    // Вызывать каждый кадр (в timerCallback)
    void update()
    {
        if (trauma > 0.0f) {
            // Линейное затухание (выглядит лучше экспоненциального для тряски)
            trauma -= decaySpeed; 
            if (trauma < 0.0f) trauma = 0.0f;
        }
    }

    // Добавить удар (вызывать, когда Transient Detector > threshold)
    // impact: 0.0 .. 1.0 (сила удара)
    void addImpact(float impact)
    {
        // Добавляем травму, но не выше 1.0
        trauma = std::min(1.0f, trauma + impact);
    }

    // Получить текущее смещение для setTransform
    juce::Point<float> getShakeOffset(float maxShakePixels = 15.0f)
    {
        if (trauma <= 0.0f) return { 0.0f, 0.0f };

        // Магия геймдева: Shake = Trauma^2 (чтобы сильные удары чувствовались мощно)
        float shake = trauma * trauma;
        
        // Вращение угла
        float angle = random.nextFloat() * juce::MathConstants<float>::twoPi;
        
        float offsetX = std::cos(angle) * shake * maxShakePixels;
        float offsetY = std::sin(angle) * shake * maxShakePixels;

        return { offsetX, offsetY };
    }
    
    // Получить текущую яркость для вспышки (Flash)
    float getFlashAlpha() const {
        return trauma * trauma * 0.3f; // Макс 30% белого
    }

private:
    float trauma = 0.0f; // Текущий уровень тряски (0..1)
    float decaySpeed = 0.04f; // Скорость затухания
    juce::Random random;
};
