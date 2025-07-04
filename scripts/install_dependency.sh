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

# Help message
show_help() {
	echo "Usage: $0 [OPTIONS]"
	echo "Install dependencies for libCacheSim"
	echo
	echo "Options:"
	echo "  -h, --help           Show this help message"
	echo "  -c, --cmake-only     Install only CMake"
	echo "  -z, --zstd-only      Install only Zstd"
	echo "  -x, --xgboost-only   Install only XGBoost"
	echo "  -l, --lightgbm-only  Install only LightGBM"
	echo
	echo "If no options are specified, all dependencies will be installed"
}

# Parse command line arguments
INSTALL_ALL=true
INSTALL_CMAKE=false
INSTALL_ZSTD=false
INSTALL_XGBOOST=false
INSTALL_LIGHTGBM=false

while [[ $# -gt 0 ]]; do
	case $1 in
	-h | --help)
		show_help
		exit 0
		;;
	-c | --cmake-only)
		INSTALL_ALL=false
		INSTALL_CMAKE=true
		shift
		;;
	-z | --zstd-only)
		INSTALL_ALL=false
		INSTALL_ZSTD=true
		shift
		;;
	-x | --xgboost-only)
		INSTALL_ALL=false
		INSTALL_XGBOOST=true
		shift
		;;
	-l | --lightgbm-only)
		INSTALL_ALL=false
		INSTALL_LIGHTGBM=true
		shift
		;;
	*)
		log_error "Unknown option: $1"
		show_help
		exit 1
		;;
	esac
done

# Store current directory
CURR_DIR=$(pwd)

# Check if running with sudo
if [[ ${EUID} -eq 0 ]]; then
	log_warn "Please do not run this script as root/sudo"
	exit 1
fi

# OS detection and setup
setup_ubuntu() {
	log_info "Setting up Ubuntu dependencies..."
	sudo apt update
	sudo apt install -yqq build-essential google-perftools xxhash ninja-build
	sudo apt install -yqq libglib2.0-dev libunwind-dev
	sudo apt install -yqq libgoogle-perftools-dev
}

setup_centos() {
	log_info "Setting up CentOS dependencies..."
	sudo yum install centos-release-scl
	sudo yum install -y glib2-devel google-perftools-devel
	# For the older llvm-toolset-7 (Clang 5.0.1)
	sudo yum install llvm-toolset-7-clang-tools-extra
	# Or for llvm-toolset-10 (Clang 10)
	# sudo yum install llvm-toolset-10.0-clang-tools-extra
	sudo scl enable llvm-toolset-10 bash
}

setup_macOS() {
	log_info "Setting up macOS dependencies..."
	if ! command -v brew &>/dev/null; then
		log_error "Homebrew is not installed. Please install Homebrew first."
		exit 1
	fi
	brew install glib google-perftools argp-standalone xxhash llvm wget cmake ninja zstd xgboost lightgbm
}

# Install CMake
install_cmake() {
	log_info "Installing CMake..."
	local cmake_version="3.31.0"
	local cmake_dir="${HOME}/software/cmake"

	pushd /tmp/ >/dev/null
	if [[ ! -f "cmake-${cmake_version}-linux-x86_64.sh" ]]; then
		wget "https://github.com/Kitware/CMake/releases/download/v${cmake_version}/cmake-${cmake_version}-linux-x86_64.sh"
	fi

	mkdir -p "${cmake_dir}" 2>/dev/null || true
	bash "cmake-${cmake_version}-linux-x86_64.sh" --skip-license --prefix="${cmake_dir}"

	# Add to shell config files if not already present
	for shell_rc in "${HOME}/.bashrc" "${HOME}/.zshrc"; do
		# trunk-ignore(shellcheck/SC2016)
		if [[ -f ${shell_rc} ]] && ! grep -q 'PATH=$HOME/software/cmake/bin:$PATH' "${shell_rc}"; then
			# trunk-ignore(shellcheck/SC2016)
			echo 'export PATH=$HOME/software/cmake/bin:$PATH' >>"${shell_rc}"
		fi
	done

	# Source the updated PATH
	export PATH="${cmake_dir}/bin:${PATH}"
	popd >/dev/null
}

# Install XGBoost
install_xgboost() {
	log_info "Installing XGBoost..."
	pushd /tmp/ >/dev/null
	if [[ ! -d "xgboost" ]]; then
		git clone --recursive https://github.com/dmlc/xgboost
	fi
	pushd xgboost >/dev/null
	mkdir -p build
	pushd build >/dev/null
	cmake -G Ninja ..
	ninja
	sudo ninja install
	popd >/dev/null
	popd >/dev/null
	popd >/dev/null
}

# Install LightGBM
install_lightgbm() {
	log_info "Installing LightGBM..."
	pushd /tmp/ >/dev/null
	if [[ ! -d "LightGBM" ]]; then
		git clone --recursive https://github.com/microsoft/LightGBM
	fi
	pushd LightGBM >/dev/null
	mkdir -p build
	pushd build >/dev/null
	cmake -G Ninja ..
	ninja
	sudo ninja install
	popd >/dev/null
	popd >/dev/null
	popd >/dev/null
}

# Install Zstd
install_zstd() {
	log_info "Installing Zstd..."
	local zstd_version="1.5.0"
	# Check if Zstd exists
	if command -v zstd &>/dev/null; then
		local installed_version
		installed_version=$(zstd --version | grep -oE 'v[0-9.]+' | sed 's/^v//')
		if [[ $installed_version == "$zstd_version" ]]; then
			log_info "Zstd version $zstd_version already installed."
			return 0
		else
			log_info "Zstd is installed but version is $installed_version (expecting $zstd_version). Proceeding to install..."
		fi
	fi
	# Install
	pushd /tmp/ >/dev/null
	if [[ ! -f "zstd-${zstd_version}.tar.gz" ]]; then
		wget "https://github.com/facebook/zstd/releases/download/v${zstd_version}/zstd-${zstd_version}.tar.gz"
		tar xf "zstd-${zstd_version}.tar.gz"
	fi
	pushd "zstd-${zstd_version}/build/cmake/" >/dev/null
	mkdir -p _build
	pushd _build >/dev/null
	cmake -G Ninja ..
	ninja
	sudo ninja install
	popd >/dev/null
	popd >/dev/null
	popd >/dev/null
}

# Main installation logic
main() {
	# Detect OS and setup basic dependencies
	if uname -a | grep -qE "Ubuntu|Debian|WSL"; then
		setup_ubuntu
	elif uname -a | grep -q Darwin; then
		setup_macOS
	else
		setup_centos
	fi

	# Install requested components only on non macOS computers
	# Libraries already installed on macOS using brew in setup_macOS
	if ! uname -a | grep -q Darwin; then
		if [[ ${INSTALL_ALL} == true ]] || [[ ${INSTALL_CMAKE} == true ]]; then
			install_cmake
		fi

		if [[ ${INSTALL_ALL} == true ]] || [[ ${INSTALL_ZSTD} == true ]]; then
			install_zstd
		fi

		if [[ ${INSTALL_ALL} == true ]] || [[ ${INSTALL_XGBOOST} == true ]]; then
			if [[ ${GITHUB_ACTIONS-} != "true" ]]; then
				install_xgboost
			fi
		fi

		if [[ ${INSTALL_ALL} == true ]] || [[ ${INSTALL_LIGHTGBM} == true ]]; then
			if [[ ${GITHUB_ACTIONS-} != "true" ]]; then
				install_lightgbm
			fi
		fi
	fi

	log_info "Installation completed successfully!"
}

# Run main function
main
