#!/bin/bash 

setup_ubuntu() {
	sudo apt update
	sudo apt install -yqq libglib2.0-dev libgoogle-perftools-dev build-essential cmake google-perftools xxhash
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
		make -j
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
		make -j
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
    make -j
    sudo make install
}

CURR_DIR=$(pwd)

if [  -n "$(uname -a | grep Ubuntu)" ]; then
	setup_ubuntu
elif [  -n "$(uname -a | grep Darwin)" ]; then
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
