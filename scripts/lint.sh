#!/bin/bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Check if clang-tidy is installed
if ! command -v clang-tidy &> /dev/null; then
    echo -e "${RED}Error: clang-tidy is not installed${NC}"
    echo "Please install clang-tidy:"
    echo "  Ubuntu/Debian: sudo apt install clang-tidy"
    echo "  macOS: brew install llvm"
    exit 1
fi

# Function to print usage
usage() {
    echo "Usage: $0 [options]"
    echo "Options:"
    echo "  -h, --help        Show this help message"
    echo "  -f, --fix         Apply suggested fixes"
    echo "  -p, --path PATH   Path to compile_commands.json"
    echo "  --all            Check all files (default: only changed files)"
    exit 1
}

# Parse arguments
FIX=0
CHECK_ALL=0
COMPILE_COMMANDS_DIR="_build"

while [[ $# -gt 0 ]]; do
    case $1 in
        -h|--help)
            usage
        ;;
        -f|--fix)
            FIX=1
            shift
        ;;
        -p|--path)
            COMPILE_COMMANDS_DIR="$2"
            shift 2
        ;;
        --all)
            CHECK_ALL=1
            shift
        ;;
        *)
            echo -e "${RED}Unknown option: $1${NC}"
            usage
        ;;
    esac
done

# Ensure we're in the project root
cd "${PROJECT_ROOT}"

# Check if compile_commands.json exists
if [ ! -f "${COMPILE_COMMANDS_DIR}/compile_commands.json" ]; then
    echo -e "${YELLOW}Warning: compile_commands.json not found in ${COMPILE_COMMANDS_DIR}${NC}"
    echo "Building project to generate compile_commands.json..."
    
    # Create build directory if it doesn't exist
    mkdir -p "${COMPILE_COMMANDS_DIR}"
    
    # Generate compile_commands.json
    pushd "${COMPILE_COMMANDS_DIR}" > /dev/null
    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON ..
    popd > /dev/null
fi

# Get list of files to check
if [ $CHECK_ALL -eq 1 ]; then
    # Find all C/C++ source files
    FILES=$(find libCacheSim -type f \( -name "*.c" -o -name "*.cpp" -o -name "*.cc" -o -name "*.h" -o -name "*.hpp" \))
else
    # Get only changed files
    FILES=$(git diff --name-only HEAD | grep -E "\.(c|cpp|cc|h|hpp)$" || true)
    if [ -z "$FILES" ]; then
        echo -e "${YELLOW}No C/C++ files changed${NC}"
        exit 0
    fi
fi

# Run clang-tidy
echo -e "${GREEN}Running clang-tidy...${NC}"
ERRORS=0
for file in $FILES; do
    echo "Checking $file..."
    if [ $FIX -eq 1 ]; then
        # Run with fix and create backup files
        clang-tidy -p "${COMPILE_COMMANDS_DIR}" --fix "$file" || ERRORS=$?
    else
        clang-tidy -p "${COMPILE_COMMANDS_DIR}" "$file" || ERRORS=$?
    fi
done

if [ $ERRORS -eq 0 ]; then
    echo -e "${GREEN}No linting errors found${NC}"
else
    echo -e "${RED}Linting errors found${NC}"
    exit 1
fi