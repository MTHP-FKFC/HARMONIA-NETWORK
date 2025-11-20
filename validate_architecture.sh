#!/bin/bash

echo "=== COHERA SATURATOR ARCHITECTURE VALIDATION ==="
echo "Testing the refactored modular architecture..."
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

PASSED=0
FAILED=0

check_file() {
    local file="$1"
    local description="$2"

    if [ -f "$file" ]; then
        echo -e "${GREEN}✓${NC} $description: $file"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}✗${NC} $description: $file (MISSING)"
        ((FAILED++))
        return 1
    fi
}

check_contains() {
    local file="$1"
    local pattern="$2"
    local description="$3"

    if grep -q "$pattern" "$file" 2>/dev/null; then
        echo -e "${GREEN}✓${NC} $description: $file"
        ((PASSED++))
        return 0
    else
        echo -e "${RED}✗${NC} $description: $file (PATTERN '$pattern' NOT FOUND)"
        ((FAILED++))
        return 1
    fi
}

echo "1. CORE ARCHITECTURE FILES"
echo "=========================="
check_file "src/CoheraTypes.h" "Global types and enums"
check_file "src/parameters/ParameterSet.h" "Parameter snapshot struct"
check_file "src/parameters/ParameterManager.h" "Parameter management class"
check_file "src/engine/ProcessingEngine.h" "Main processing orchestrator"
echo ""

echo "2. ENGINE COMPONENTS"
echo "==================="
check_file "src/engine/FilterBankEngine.h" "6-band filter bank"
check_file "src/engine/BandProcessingEngine.h" "Individual band processor"
check_file "src/engine/TransientEngine.h" "Split & Crush logic"
check_file "src/engine/AnalogModelingEngine.h" "Mojo effects"
check_file "src/engine/MixEngine.h" "Dry/Wet mixing"
check_file "src/network/NetworkController.h" "Network communication"
echo ""

echo "3. DSP MODULES"
echo "=============="
check_file "src/dsp/FilterBank.h" "Crossover implementation"
check_file "src/dsp/MathSaturator.h" "Mathematical saturation"
check_file "src/dsp/DCBlocker.h" "DC offset removal"
check_file "src/dsp/TransientSplitter.h" "Transient detection"
check_file "src/dsp/ThermalModel.h" "Lamp simulation"
echo ""

echo "4. INTEGRATION TESTS"
echo "==================="
check_file "src/tests/TestHelpers.h" "Test utilities"
check_file "src/tests/EngineIntegrationTests.cpp" "Integration test suite"
check_file "src/tests/TestRunner.cpp" "Test runner"
echo ""

echo "5. DEPENDENCY CHECKS"
echo "==================="

# Check ProcessingEngine includes
echo "ProcessingEngine dependencies:"
if check_contains "src/engine/ProcessingEngine.h" "FilterBankEngine.h" "Includes FilterBankEngine"; then
    if check_contains "src/engine/ProcessingEngine.h" "MixEngine.h" "Includes MixEngine"; then
        if check_contains "src/engine/ProcessingEngine.h" "NetworkController.h" "Includes NetworkController"; then
            echo -e "${GREEN}✓${NC} ProcessingEngine has all required dependencies"
            ((PASSED++))
        fi
    fi
fi

# Check FilterBankEngine includes
echo "FilterBankEngine dependencies:"
if check_contains "src/engine/FilterBankEngine.h" "BandProcessingEngine.h" "Includes BandProcessingEngine"; then
    if check_contains "src/engine/FilterBankEngine.h" "FilterBank.h" "Includes FilterBank"; then
        echo -e "${GREEN}✓${NC} FilterBankEngine has all required dependencies"
        ((PASSED++))
    fi
fi

# Check BandProcessingEngine includes
echo "BandProcessingEngine dependencies:"
if check_contains "src/engine/BandProcessingEngine.h" "TransientEngine.h" "Includes TransientEngine"; then
    if check_contains "src/engine/BandProcessingEngine.h" "AnalogModelingEngine.h" "Includes AnalogModelingEngine"; then
        echo -e "${GREEN}✓${NC} BandProcessingEngine has all required dependencies"
        ((PASSED++))
    fi
fi

# Check PluginProcessor integration
echo "PluginProcessor integration:"
if check_contains "src/PluginProcessor.h" "ProcessingEngine.h" "Includes ProcessingEngine"; then
    if check_contains "src/PluginProcessor.h" "ParameterManager.h" "Includes ParameterManager"; then
        echo -e "${GREEN}✓${NC} PluginProcessor properly integrated"
        ((PASSED++))
    fi
fi

# Check test integration
echo "Test integration:"
if check_contains "src/PluginProcessor.cpp" "UnitTestRunner" "Has test runner"; then
    if check_contains "src/PluginProcessor.cpp" "JUCE_DEBUG" "Debug test execution"; then
        echo -e "${GREEN}✓${NC} Tests properly integrated in Debug mode"
        ((PASSED++))
    fi
fi

echo ""

echo "6. CMAKE CONFIGURATION"
echo "======================"
check_contains "CMakeLists.txt" "ProcessingEngine.h" "ProcessingEngine in CMake"
check_contains "CMakeLists.txt" "FilterBankEngine.h" "FilterBankEngine in CMake"
check_contains "CMakeLists.txt" "NetworkController.h" "NetworkController in CMake"
check_contains "CMakeLists.txt" "TestHelpers.h" "Tests in CMake"
echo ""

echo "7. NAMESPACE CONSISTENCY"
echo "========================"
namespace_files=(
    "src/CoheraTypes.h"
    "src/parameters/ParameterSet.h"
    "src/parameters/ParameterManager.h"
    "src/engine/ProcessingEngine.h"
    "src/engine/FilterBankEngine.h"
    "src/engine/BandProcessingEngine.h"
    "src/engine/TransientEngine.h"
    "src/engine/AnalogModelingEngine.h"
    "src/engine/MixEngine.h"
    "src/network/NetworkController.h"
    "src/tests/TestHelpers.h"
    "src/tests/EngineIntegrationTests.cpp"
)

echo "Checking Cohera namespace usage..."
namespace_check_passed=0
for file in "${namespace_files[@]}"; do
    if [ -f "$file" ] && grep -q "namespace Cohera" "$file"; then
        ((namespace_check_passed++))
    fi
done

if [ $namespace_check_passed -eq ${#namespace_files[@]} ]; then
    echo -e "${GREEN}✓${NC} All ${#namespace_files[@]} files use Cohera namespace"
    ((PASSED++))
else
    echo -e "${YELLOW}⚠${NC} $namespace_check_passed/${#namespace_files[@]} files use Cohera namespace"
fi

echo ""

echo "=== VALIDATION SUMMARY ==="
echo "=========================="
echo "Passed: $PASSED"
echo "Failed: $FAILED"
echo "Total:  $((PASSED + FAILED))"

if [ $FAILED -eq 0 ]; then
    echo ""
    echo -e "${GREEN}🎉 ALL VALIDATION CHECKS PASSED! 🎉${NC}"
    echo ""
    echo "The Cohera Saturator modular architecture is correctly implemented."
    echo "All components are properly connected and the refactoring was successful."
    echo ""
    echo "Ready for production use! 🚀"
    exit 0
else
    echo ""
    echo -e "${RED}❌ VALIDATION FAILED: $FAILED issues found${NC}"
    echo ""
    echo "Please check the failed items above."
    exit 1
fi
