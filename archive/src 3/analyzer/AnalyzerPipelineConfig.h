#pragma once

namespace Analyzer {

enum class MaterialType
{
    Auto,           // Автоматическое определение
    KickHeavy,      // Доминируют бас-барабаны
    SnareHeavy,     // Доминируют малые барабаны
    CymbalHeavy,    // Доминируют тарелки/хай-хэты
    VocalHeavy,     // Доминируют вокалы
    BassHeavy,      // Доминирует бас
    Percussive,     // Перкуссия в целом
    Synthetic,      // Синтетические звуки
    MixComplex      // Сложный микс
};

} // namespace Analyzer
