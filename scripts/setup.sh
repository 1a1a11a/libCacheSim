
CURR_DIR=$(pwd)

setup_ubuntu() {
	sudo apt update
	sudo apt install libglib2.0-dev libgoogle-perftools-dev build-essential cmake google-perftools
}

setup_centos() {
	sudo yum install glib2-devel google-perftools-devel
}

setup_macOS() {
	brew install glib google-perftools

}

setup_xgboost() {
    pushd /tmp/
	git clone --recursive https://github.com/dmlc/xgboost
	pushd xgboost
	mkdir build
	pushd build
	cmake ..
	make
	sudo make install
}

setup_lightgbm() {
    pushd /tmp/
	git clone --recursive https://github.com/microsoft/LightGBM
	pushd LightGBM
	mkdir build
	pushd build
	cmake ..
	make
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
    make
    sudo make install
}


setup_ubuntu
setup_xgboost
setup_lightgbm
setup_zstd







