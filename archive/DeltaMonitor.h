#pragma once

class DeltaMonitor
{
public:
    void setActive(bool shouldBeActive)
    {
        active = shouldBeActive;
    }

    bool isActive() const { return active; }

    // Возвращает итоговый сэмпл для выхода
    // dry: Исходный (задержанный) сигнал
    // wet: Обработанный сигнал
    // mixed: Результат ручки Mix (Dry + Wet)
    float process(float dry, float wet, float mixed)
    {
        if (active)
        {
            // Delta = Разница между Входом и Выходом.
            // Мы берем mixed (то, что идет на выход) и вычитаем dry.
            // Остается только то, что "добавил" плагин (гармоники, компрессия).
            // Умножаем на 1.0, чтобы уровень был честным.
            return mixed - dry;
        }

        // Если мониторинг выключен - возвращаем обычный микс
        return mixed;
    }

private:
    bool active = false;
};
