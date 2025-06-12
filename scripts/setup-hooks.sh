#!/bin/bash
set -euo pipefail

SCRIPT_DIR=$(dirname "$(readlink -f "$0")")
REPO_ROOT=$(dirname "$SCRIPT_DIR")
HOOKS_DIR="$REPO_ROOT/.git/hooks"

echo "Installing git hooks..."

# Create pre-commit hook
cat > "$HOOKS_DIR/pre-commit" << 'EOL'
#!/bin/bash
set -e

# Colors for better output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Enable running the commit even with issues using environment variable
# Usage: SKIP_LINT=1 git commit -m "..."
if [ -n "$SKIP_LINT" ]; then
    echo -e "${YELLOW}Skipping pre-commit checks due to SKIP_LINT environment variable${NC}"
    exit 0
fi

echo -e "${BLUE}Running pre-commit linting checks...${NC}"
START_TIME=$(date +%s)

# Store the root of the git repository
GIT_ROOT=$(git rev-parse --show-toplevel)
cd "$GIT_ROOT"

# Get a list of staged files
STAGED_FILES=$(git diff --cached --name-only --diff-filter=ACM | grep -E '\.([ch](pp)?|cc)$' || true)

if [ -z "$STAGED_FILES" ]; then
    echo -e "${GREEN}No C/C++ files to check.${NC}"
    exit 0
fi

NUM_FILES=$(echo "$STAGED_FILES" | wc -l)
echo -e "${BLUE}Found ${NUM_FILES} files to check${NC}"

# Create a temporary build directory for linting
TEMP_BUILD_DIR=$(mktemp -d)
LOG_DIR="$TEMP_BUILD_DIR/logs"
mkdir -p "$LOG_DIR"

# Create a cleanup function
cleanup() {
    echo -e "${BLUE}Cleaning up temporary files...${NC}"
    rm -rf "$TEMP_BUILD_DIR"
}
trap cleanup EXIT

# Configure the project with the same warning flags
echo -e "${BLUE}Configuring temporary build for linting...${NC}"
mkdir -p "$TEMP_BUILD_DIR"
cd "$TEMP_BUILD_DIR"
CMAKE_LOG="$LOG_DIR/cmake.log"
cmake -DCMAKE_BUILD_TYPE=Debug \
      -DCMAKE_C_FLAGS="-Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-unused-but-set-variable -Wpedantic -Wformat=2 -Wformat-security -Wshadow -Wwrite-strings -Wstrict-prototypes -Wold-style-definition -Wredundant-decls -Wnested-externs -Wmissing-include-dirs" \
      -DCMAKE_CXX_FLAGS="-Wall -Wextra -Werror -Wno-unused-variable -Wno-unused-function -Wno-unused-parameter -Wno-unused-but-set-variable -Wno-pedantic -Wformat=2 -Wformat-security -Wshadow -Wwrite-strings -Wmissing-include-dirs" \
      -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
      "$GIT_ROOT" > "$CMAKE_LOG" 2>&1 || {
          echo -e "${RED}Failed to configure build. See $CMAKE_LOG for details.${NC}"
          exit 1
      }

# Determine the number of processors
NUM_CPUS=$(nproc)
MAX_JOBS=$((NUM_CPUS > 8 ? 8 : NUM_CPUS))  # Limit to 8 concurrent jobs to avoid overloading

# Function to show elapsed time
show_elapsed() {
    local elapsed=$(($(date +%s) - START_TIME))
    echo -e "${BLUE}Time elapsed: ${elapsed}s${NC}"
}

# Check if clang-format exists
if command -v clang-format >/dev/null 2>&1; then
    echo -e "${BLUE}Running clang-format checks...${NC}"
    FORMAT_TIME=$(date +%s)
    FORMAT_LOG="$LOG_DIR/format.log"
    FORMAT_ERRORS=0

    cd "$GIT_ROOT"
    for FILE in $STAGED_FILES; do
        echo -e "  Checking formatting for ${BLUE}$FILE${NC}"
        if ! clang-format --dry-run --Werror "$FILE" >> "$FORMAT_LOG" 2>&1; then
            echo -e "  ${RED}Formatting issues in $FILE${NC}"
            FORMAT_ERRORS=$((FORMAT_ERRORS + 1))
        fi
    done

    if [ $FORMAT_ERRORS -gt 0 ]; then
        echo -e "${RED}Found formatting issues in $FORMAT_ERRORS files. Commit blocked.${NC}"
        echo -e "  You can fix these issues with: clang-format -i <file>"
        echo -e "  Or bypass this check with: SKIP_LINT=1 git commit"
        echo -e "  See format logs at: $PERSISTENT_LOG_DIR/latest/format.log"
        exit 1
    else
        echo -e "${GREEN}Formatting check passed.${NC}"
    fi
    echo -e "${BLUE}Format check completed in $(($(date +%s) - FORMAT_TIME))s${NC}"
    cd "$TEMP_BUILD_DIR"
else
    echo -e "${YELLOW}clang-format not found, skipping format checks${NC}"
fi

# Check if clang-tidy exists
if command -v clang-tidy >/dev/null 2>&1; then
    echo -e "${BLUE}Running clang-tidy checks in parallel (max $MAX_JOBS jobs)...${NC}"
    TIDY_TIME=$(date +%s)
    
    # Create a temporary file to capture errors
    ERROR_LOG="$LOG_DIR/tidy_errors.log"
    touch "$ERROR_LOG"
    
    # Create a file to track error count from subshells
    ERROR_COUNT_FILE="$LOG_DIR/error_count.txt"
    echo "0" > "$ERROR_COUNT_FILE"
    
    # Run clang-tidy on each file in parallel
    PIDS=()
    FILES_CHECKED=0
    
    for FILE in $STAGED_FILES; do
        # If we've hit the max jobs limit, wait for one to finish
        while [ ${#PIDS[@]} -ge $MAX_JOBS ]; do
            # Wait for any job to finish, then remove it from the array
            for i in "${!PIDS[@]}"; do
                if ! kill -0 ${PIDS[$i]} 2>/dev/null; then
                    unset "PIDS[$i]"
                fi
            done
            # Only wait a bit before checking again
            sleep 0.1
        done
        
        # Track progress
        FILES_CHECKED=$((FILES_CHECKED + 1))
        PROGRESS=$((FILES_CHECKED * 100 / NUM_FILES))
        echo -e "  [${PROGRESS}%] Checking ${BLUE}$FILE${NC} with clang-tidy"
        
        # Create a log file for this specific file
        TIDY_LOG="$LOG_DIR/tidy_$(basename "$FILE").log"
        
        # Run clang-tidy in background
        (
            # Run clang-tidy with a selected set of checks
            # Excluded some checks that might be too noisy
            if ! clang-tidy -p="$TEMP_BUILD_DIR" \
                -checks='-*,bugprone-*,cert-*,clang-analyzer-*,cppcoreguidelines-*,performance-*,portability-*,readability-*,-readability-magic-numbers,-readability-braces-around-statements,-cppcoreguidelines-avoid-magic-numbers,-readability-identifier-length,-clang-analyzer-security.insecureAPI.DeprecatedOrUnsafeBufferHandling,-clang-diagnostic-unused-command-line-argument' \
                "$GIT_ROOT/$FILE" > "$TIDY_LOG" 2>&1; then
                echo -e "  ${YELLOW}clang-tidy found issues in $FILE (see $TIDY_LOG)${NC}" 
                echo "$FILE" >> "$ERROR_LOG"
                
                # Update the error count in the shared file
                flock "$ERROR_COUNT_FILE" bash -c "
                    COUNT=\$(cat \"$ERROR_COUNT_FILE\")
                    echo \$((COUNT + 1)) > \"$ERROR_COUNT_FILE\"
                "
            fi
        ) &
        
        # Store the PID
        PIDS+=($!)
    done
    
    # Wait for all remaining jobs to finish with a progress indicator
    echo -e "${BLUE}Waiting for all clang-tidy jobs to complete...${NC}"
    while [ ${#PIDS[@]} -gt 0 ]; do
        for i in "${!PIDS[@]}"; do
            if ! kill -0 ${PIDS[$i]} 2>/dev/null; then
                unset "PIDS[$i]"
            fi
        done
        echo -ne "  Remaining jobs: ${#PIDS[@]}/$MAX_JOBS \r"
        sleep 1
    done
    echo -e "\n${BLUE}All clang-tidy jobs completed.${NC}"
    
    # Read the final error count
    FILES_WITH_ISSUES=$(cat "$ERROR_COUNT_FILE")
    
    # Report the results
    if [ -s "$ERROR_LOG" ]; then
        echo -e "${RED}clang-tidy found issues in $FILES_WITH_ISSUES files. Commit blocked.${NC}"
        echo -e "  Logs are available at: $LOG_DIR/latest/tidy_*.log"
        echo -e "  List of files with issues: $LOG_DIR/latest/tidy_errors.log"
        echo -e "  Or bypass this check with: SKIP_LINT=1 git commit"
        exit 1
    else
        echo -e "${GREEN}clang-tidy check passed successfully.${NC}"
    fi
    echo -e "${BLUE}clang-tidy checks completed in $(($(date +%s) - TIDY_TIME))s${NC}"
else
    echo -e "${YELLOW}clang-tidy not found, skipping static analysis${NC}"
fi

# # Check for compilation warnings
# echo -e "${BLUE}Checking for compilation warnings...${NC}"
# COMPILE_TIME=$(date +%s)
# cd "$TEMP_BUILD_DIR"

# COMPILE_LOG="$LOG_DIR/compile.log"
# if make -j$MAX_JOBS > "$COMPILE_LOG" 2>&1; then
#     echo -e "${GREEN}Compilation successful!${NC}"
# else
#     echo -e "${RED}Compilation failed. Please fix the warnings before committing.${NC}"
#     echo -e "  See compilation log at: $LOG_DIR/latest/compile.log"
#     echo -e "  You can also bypass this check with SKIP_LINT=1 git commit"
#     exit 1
# fi
# echo -e "${BLUE}Compilation check completed in $(($(date +%s) - COMPILE_TIME))s${NC}"

# Show total time
TOTAL_TIME=$(($(date +%s) - START_TIME))
echo -e "${GREEN}All linting checks completed successfully in ${TOTAL_TIME}s!${NC}"
exit 0
EOL

# Make hooks executable
chmod +x "$HOOKS_DIR/pre-commit"

echo "Git hooks installed successfully!"
echo "The pre-commit hook will now run automatically on each commit to check for linting issues."
echo "You can bypass the checks with: SKIP_LINT=1 git commit"
echo "Linting logs are stored in .lint-logs/ for future reference."
echo "Note: clang-tidy and clang-format will be used if they're installed on your system." 