#!/bin/bash

set -euo pipefail # Enable strict error checking

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
	echo -e "${GREEN}[INFO]${NC} $1"
}

log_warn() {
	echo -e "${YELLOW}[WARN]${NC} $1"
}

log_error() {
	echo -e "${RED}[ERROR]${NC} $1" >&2
}

# Cleanup function
cleanup() {
	local exit_code=$?
	if [[ ${exit_code} -ne 0 ]]; then
		log_error "Script failed with exit code ${exit_code}"
	fi
	cd "${CURR_DIR}"
	exit "${exit_code}"
}

trap cleanup EXIT

# Store current directory
CURR_DIR=$(pwd)
FILE_DIR=$(dirname "$0")

# Main installation logic
main() {
	# Detect OS and setup basic dependencies
	if [[ -f /etc/os-release ]] && grep -qiE 'ubuntu|debian' /etc/os-release; then
		sudo apt install -yqq gdb valgrind clang-tidy clang-format ninja-build
		# node js dependency for libCacheSim-js
		sudo apt install -yqq nodejs npm
	# trunk-ignore(shellcheck/SC2312)
	elif [[ $(uname -a) == *"Darwin"* ]]; then
		# note that clang-format version on macOS might be newer than the one in ubuntu, and it can cause different formatting results
		brew install gdb clang-format flock nodejs npm ninja
	elif grep -qi 'microsoft' /proc/version 2>/dev/null; then
		# WSL detection
		sudo apt install -yqq gdb valgrind clang-tidy clang-format ninja-build
	else
		log_error "Unsupported operating system. Only Ubuntu, Debian, WSL, and macOS are supported. Some dependencies may not be installed."
	fi

	bash "${FILE_DIR}"/setup_hooks.sh

	log_info "Installation completed successfully!"
}

# Run main function
main
