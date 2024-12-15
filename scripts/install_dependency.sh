#!/bin/bash 

set -eu # Enable error checking and command tracing

setup_ubuntu() {
	sudo apt update
	sudo apt install -yqq build-essential google-perftools xxhash
	
	sudo apt install -yqq libglib2.0-dev libunwind-dev
	sudo apt install -yqq libgoogle-perftools-dev
}

setup_centos() {
	sudo yum install -y glib2-devel google-perftools-devel
}

setup_macOS() {
	brew install glib google-perftools argp-standalone xxhash
}

install_cmake() {
	pushd /tmp/;
	wget https://github.com/Kitware/CMake/releases/download/v3.31.0/cmake-3.31.0-linux-x86_64.sh;
	mkdir -p $HOME/software/cmake 2>/dev/null || true;
	bash cmake-3.31.0-linux-x86_64.sh --skip-license --prefix=$HOME/software/cmake;
	echo 'export PATH=$HOME/software/cmake/bin:$PATH' >> $HOME/.bashrc;
	echo 'export PATH=$HOME/software/cmake/bin:$PATH' >> $HOME/.zshrc;
	source $HOME/.bashrc;
	source $HOME/.zshrc;
	popd;
}

install_xgboost() {
    pushd /tmp/
	if [ ! -d "xgboost" ]; then
		git clone --recursive https://github.com/dmlc/xgboost
	fi
	pushd xgboost
	mkdir build || true
	pushd build
	cmake ..
	if [[ ${GITHUB_ACTIONS:-} == "true" ]]; then
		make
	else
		make -j $(nproc)
	fi
	sudo make install
}

install_lightgbm() {
    pushd /tmp/
	if [ ! -d "LightGBM" ]; then
		git clone --recursive https://github.com/microsoft/LightGBM
	fi
	pushd LightGBM
	mkdir build || true
	pushd build
	cmake ..
	if [[ ${GITHUB_ACTIONS:-} == "true" ]]; then
		make
	else
		make -j $(nproc)
	fi
	sudo make install
}

install_zstd() {
    pushd /tmp/;
	if [ ! -f "zstd-1.5.0.tar.gz" ]; then 
	    wget https://github.com/facebook/zstd/releases/download/v1.5.0/zstd-1.5.0.tar.gz
	    tar xvf zstd-1.5.0.tar.gz;
	fi
    pushd zstd-1.5.0/build/cmake/
    mkdir _build || true
    pushd _build/;
    cmake ..
    make -j $(nproc)
    sudo make install
}

CURR_DIR=$(pwd)

if [ -n "$(uname -a | grep Ubuntu)" ] || [ -n "$(uname -a | grep Debian)" ] || [ -n "$(uname -a | grep WSL)" ]; then
    setup_ubuntu
elif [ -n "$(uname -a | grep Darwin)" ]; then
    setup_macOS
else
    setup_centos
fi 

install_cmake
install_zstd

if [[ ! ${GITHUB_ACTIONS:-} == "true" ]]; then
	install_xgboost
	install_lightgbm
fi

cd $CURR_DIR
