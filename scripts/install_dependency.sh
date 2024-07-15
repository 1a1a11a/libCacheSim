#!/bin/bash 

set -eu # Enable error checking and command tracing

setup_ubuntu() {
	sudo apt update
	sudo apt install -yqq build-essential cmake google-perftools xxhash
	
	sudo apt install -yqq libglib2.0-dev libunwind-dev
	sudo apt install -yqq libgoogle-perftools-dev
}

setup_centos() {
	sudo yum install glib2-devel google-perftools-devel
}

setup_macOS() {
	brew install glib google-perftools argp-standalone xxhash
}

setup_xgboost() {
    pushd /tmp/
	git clone --recursive https://github.com/dmlc/xgboost
	pushd xgboost
	mkdir build
	pushd build
	cmake ..
	if [[ $GITHUB_ACTIONS == "true" ]]; then
		make
	else
		make -j $(nproc)
	fi
	sudo make install
}

setup_lightgbm() {
    pushd /tmp/
	git clone --recursive https://github.com/microsoft/LightGBM
	pushd LightGBM
	mkdir build
	pushd build
	cmake ..
	if [[ $GITHUB_ACTIONS == "true" ]]; then
		make
	else
		make -j $(nproc)
	fi
	sudo make install
}

setup_zstd() {
    pushd /tmp/
    wget https://github.com/facebook/zstd/releases/download/v1.5.0/zstd-1.5.0.tar.gz
    tar xvf zstd-1.5.0.tar.gz;
    pushd zstd-1.5.0/build/cmake/
    mkdir _build;
    pushd _build/;
    cmake ..
    make -j $(nproc)
    sudo make install
}

CURR_DIR=$(pwd)

if [ -n "$(uname -a | grep Ubuntu)" ] || [ -n "$(uname -a | grep WSL)" ]; then
    setup_ubuntu
elif [ -n "$(uname -a | grep Darwin)" ]; then
    setup_macOS
else
    setup_centos
fi 

setup_zstd

if [[ ! $GITHUB_ACTIONS == "true" ]]; then
	setup_xgboost
	setup_lightgbm
fi

cd $CURR_DIR
