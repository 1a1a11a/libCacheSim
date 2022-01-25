

setup_ubuntu() {
	sudo apt install libglib2.0-dev libgoogle-perftools-dev build-essential cmake google-perftools


}

setup_centos() {
	sudo yum install glib2-devel google-perftools-devel
}

setup_macOS() {
	brew install glib google-perftools

}

setup_xgboost() {
    cd /tmp/
	git clone --recursive https://github.com/dmlc/xgboost
	cd xgboost
	mkdir build
	cd build
	cmake ..
	make -j
	sudo make install	
}

setup_lightgbm() {
    cd /tmp/
	git clone --recursive https://github.com/microsoft/LightGBM
	cd LightGBM
	mkdir build
	cd build
	cmake ..
	make -j
	sudo make install
}

setup_zstd() {
    cd /tmp/
    wget https://github.com/facebook/zstd/releases/download/v1.5.0/zstd-1.5.0.tar.gz
    tar xvf zstd-1.5.0.tar.gz;
    cd zstd-1.5.0/build/cmake/
    mkdir _build;
    cd _build/;
    cmake ..
    make -j
    sudo make install
}

# setup_ubuntu 
setup_xgboost
setup_lightgbm
setup_zstd

