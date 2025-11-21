#include "JuceHeader.h"
#include "PluginProcessor.h"

// Эта функция создает новый экземпляр твоего плагина
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new CoheraSaturatorAudioProcessor();
}
