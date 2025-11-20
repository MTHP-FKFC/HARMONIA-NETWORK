#pragma once

#include <juce_dsp/juce_dsp.h>
#include "../CoheraTypes.h"
#include "../parameters/ParameterSet.h"

namespace Cohera {

// TEMPORARY SIMPLE VERSION FOR COMPILATION TEST
class BandProcessingEngine
{
public:
    void prepare(const juce::dsp::ProcessSpec& spec) {}
    void reset() {}

    void process(juce::dsp::AudioBlock<float>& block,
                 const ParameterSet& params,
                 float netModulation = 0.0f)
    {
        // Do nothing for now - just compilation test
    }
};

} // namespace Cohera
