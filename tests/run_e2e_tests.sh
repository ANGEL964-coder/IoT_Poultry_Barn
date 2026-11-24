#!/usr/bin/env bash
# ==============================================================================
# Universal E2E Test Runner for SDN LoRaWAN Poultry Controller
# Supports: --tier1, --tier2, --tier3, --tier4, --all
# Exit code: 0 on all tests passing, >0 on any failure.
# ==============================================================================

set -o pipefail

BASE_DIR="/home/server/CONTROLADOR_SDN"
VENV_PYTEST="${BASE_DIR}/.venv/bin/pytest"
TESTS_DIR="${BASE_DIR}/tests"

# Colors for terminal output
C_RESET="\033[0m"
C_BOLD="\033[1m"
C_CYAN="\033[1;36m"
C_GREEN="\033[1;32m"
C_RED="\033[1;31m"
C_YELLOW="\033[1;33m"
C_BLUE="\033[1;34m"

# Print banner
echo -e "${C_CYAN}==============================================================================${C_RESET}"
echo -e "${C_BOLD}   SDN LoRaWAN Poultry Controller — E2E Test Suite Runner${C_RESET}"
echo -e "${C_CYAN}==============================================================================${C_RESET}"

# Verify virtual environment pytest
if [ ! -x "${VENV_PYTEST}" ]; then
    echo -e "${C_RED}ERROR: Pytest not found in virtual environment: ${VENV_PYTEST}${C_RESET}"
    exit 1
fi

RUN_TIER1=false
RUN_TIER2=false
RUN_TIER3=false
RUN_TIER4=false

# Argument parsing
if [ "$#" -eq 0 ]; then
    RUN_TIER1=true
    RUN_TIER2=true
    RUN_TIER3=true
    RUN_TIER4=true
fi

while [ "$#" -gt 0 ]; do
    case "$1" in
        --tier1)
            RUN_TIER1=true
            shift
            ;;
        --tier2)
            RUN_TIER2=true
            shift
            ;;
        --tier3)
            RUN_TIER3=true
            shift
            ;;
        --tier4)
            RUN_TIER4=true
            shift
            ;;
        --all)
            RUN_TIER1=true
            RUN_TIER2=true
            RUN_TIER3=true
            RUN_TIER4=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [--tier1] [--tier2] [--tier3] [--tier4] [--all]"
            echo "Options:"
            echo "  --tier1    Run Tier 1 Feature Coverage Tests (F01-F17, >=85 tests)"
            echo "  --tier2    Run Tier 2 Boundary & Corner Case Tests (>=35 tests)"
            echo "  --tier3    Run Tier 3 Pairwise Combinatorial Integration Tests (>=15 tests)"
            echo "  --tier4    Run Tier 4 Real-World Application Scenarios (>=5 scenarios)"
            echo "  --all      Run entire test suite across all 4 tiers (Default)"
            exit 0
            ;;
        *)
            echo -e "${C_RED}Unknown option: $1${C_RESET}"
            echo "Usage: $0 [--tier1] [--tier2] [--tier3] [--tier4] [--all]"
            exit 2
            ;;
    esac
done

TARGET_PATHS=()

if [ "$RUN_TIER1" = true ]; then
    TARGET_PATHS+=("${TESTS_DIR}/tier1_features/")
fi

if [ "$RUN_TIER2" = true ]; then
    TARGET_PATHS+=("${TESTS_DIR}/tier2_boundaries/")
fi

if [ "$RUN_TIER3" = true ]; then
    TARGET_PATHS+=("${TESTS_DIR}/tier3_pairwise/")
fi

if [ "$RUN_TIER4" = true ]; then
    TARGET_PATHS+=("${TESTS_DIR}/tier4_scenarios/")
fi

echo -e "${C_BLUE}Executing test targets:${C_RESET}"
for target in "${TARGET_PATHS[@]}"; do
    echo -e "  • ${target}"
done
echo ""

START_TIME=$(date +%s%N)

# Execute pytest
"${VENV_PYTEST}" "${TARGET_PATHS[@]}" -v --tb=short
EXIT_CODE=$?

END_TIME=$(date +%s%N)
DURATION_MS=$(( (END_TIME - START_TIME) / 1000000 ))

echo ""
echo -e "${C_CYAN}==============================================================================${C_RESET}"
if [ $EXIT_CODE -eq 0 ]; then
    echo -e "${C_GREEN}${C_BOLD}✔ ALL E2E TEST SUITES PASSED CLEANLY (100% SUCCESS)${C_RESET}"
    echo -e "  Total Execution Time: ${DURATION_MS} ms"
    echo -e "${C_CYAN}==============================================================================${C_RESET}"
    exit 0
else
    echo -e "${C_RED}${C_BOLD}✘ TEST FAILURES DETECTED (Exit code: ${EXIT_CODE})${C_RESET}"
    echo -e "  Total Execution Time: ${DURATION_MS} ms"
    echo -e "${C_CYAN}==============================================================================${C_RESET}"
    exit $EXIT_CODE
fi
