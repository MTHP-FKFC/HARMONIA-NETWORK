#include <juce_core/juce_core.h>
#include <iostream>
#include <fstream>
#include "EngineIntegrationTests.cpp" // Include the test implementations
#include "RealWorldScenarios.cpp" // Include real-world scenario tests
#include "DCBlockerSampleRateTest.cpp"
#include "HarmonicAnalysisTest.cpp"
#include "FrequencyBalanceTest.cpp"
#include "MultiSampleRateTest.cpp"

int main(int argc, char* argv[])
{
    std::cout << "=== COHERA SATURATOR INTEGRATION TESTS ===" << std::endl;
    std::cout << "Starting test runner..." << std::endl;

    // Create log file (fallback to workspace if Desktop is not writable, e.g. sandboxed CI)
    juce::File logFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory)
                        .getChildFile("cohera_test_results.txt");

    std::ofstream logFileStream(logFile.getFullPathName().toStdString());
    if (!logFileStream.is_open())
    {
        juce::File fallback = juce::File::getCurrentWorkingDirectory()
                                .getChildFile("cohera_test_results.txt");
        logFile = fallback;
        logFileStream.open(fallback.getFullPathName().toStdString());
    }

    if (!logFileStream.is_open())
    {
        std::cout << "ERROR: Cannot create log file at Desktop or workspace!" << std::endl;
        return 1;
    }

    logFileStream << "=== COHERA SATURATOR INTEGRATION TESTS ===\n";
    logFileStream << "Started at: " << juce::Time::getCurrentTime().toString(true, true).toStdString() << "\n\n";

    // Initialize JUCE
    juce::ScopedJuceInitialiser_GUI juceInit;

    // Create test runner
    juce::UnitTestRunner runner;
    runner.setPassesAreLogged(true);

    // Run all tests
    std::cout << "Running all tests...\n";
    logFileStream << "Running all tests...\n\n";

    runner.runAllTests();

    // Get results
    int totalTests = runner.getNumResults();
    int totalFailures = 0;

    for (int i = 0; i < totalTests; ++i)
    {
        const juce::UnitTestRunner::TestResult* result = runner.getResult(i);
        if (result != nullptr && result->failures > 0)
            totalFailures += result->failures;
    }

    // Summary
    std::string summary = "\n=== TEST SUMMARY ===\n";
    if (totalFailures == 0)
    {
        summary += "ALL TESTS PASSED! ✓✓✓\n";
        summary += "Cohera Saturator architecture is working correctly.\n";
    }
    else
    {
        summary += "TESTS FAILED: " + std::to_string(totalFailures) + " total failures\n";
        summary += "Please check the implementation.\n";
    }

    summary += "\nLog saved to: " + logFile.getFullPathName().toStdString() + "\n";

    std::cout << summary;
    logFileStream << summary;

    logFileStream.close();

    return totalFailures == 0 ? 0 : 1;
}
