#include <iostream>
#include <juce_core/juce_core.h>
#include <juce_audio_processors/juce_audio_processors.h>

// Include our Industry Standard tests
#include "IndustryStandardTests.cpp"

int main(int argc, char* argv[])
{
    std::cout << "=== COHERA SATURATOR INDUSTRY STANDARD TESTS ===" << std::endl;
    std::cout << "Testing professional plugin stability and integration..." << std::endl;
    std::cout << std::endl;

    juce::ScopedJuceInitialiser_GUI juceInit;

    // Run Industry Standard tests
    juce::UnitTestRunner runner;
    runner.setPassesAreLogged(true);

    std::cout << "Running Industry Standard Tests..." << std::endl;
    std::cout << "==================================" << std::endl;

    runner.runAllTests();

    // Get results
    int totalTests = runner.getNumResults();
    int totalFailures = 0;
    int totalPasses = 0;

    for (int i = 0; i < totalTests; ++i)
    {
        const juce::UnitTestRunner::TestResult* result = runner.getResult(i);
        if (result != nullptr)
        {
            if (result->failures > 0)
                totalFailures += result->failures;
            else
                totalPasses++;
        }
    }

    std::cout << std::endl;
    std::cout << "=== INDUSTRY STANDARD TEST RESULTS ===" << std::endl;
    std::cout << "======================================" << std::endl;

    if (totalFailures == 0)
    {
        std::cout << "✅ ALL INDUSTRY STANDARD TESTS PASSED!" << std::endl;
        std::cout << "   ✓ State Persistence: Plugin settings survive save/load" << std::endl;
        std::cout << "   ✓ Variable Block Size: Handles 'Host from Hell' scenarios" << std::endl;
        std::cout << "   ✓ Parameter Smoothing: No zipper noise on automation" << std::endl;
        std::cout << std::endl;
        std::cout << "🎯 PROFESSIONAL PLUGIN CERTIFIED!" << std::endl;
        std::cout << "Your plugin meets industry standards for stability and integration." << std::endl;
        std::cout << std::endl;
        std::cout << "🏆 READY FOR PRODUCTION RELEASE! 🏆" << std::endl;
    }
    else
    {
        std::cout << "❌ INDUSTRY STANDARD TESTS FAILED: " << totalFailures << " total failures" << std::endl;
        std::cout << "Please check the implementation for stability issues." << std::endl;
    }

    std::cout << std::endl;
    std::cout << "Summary: " << totalPasses << " passed, " << totalFailures << " failed" << std::endl;

    return totalFailures == 0 ? 0 : 1;
}
