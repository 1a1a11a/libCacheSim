
## Install dependency
libCacheSim uses [camke](https://cmake.org/) build system and has a few dependencies: 
[GNOME glib](https://developer.gnome.org/glib/), 
[Google tcmalloc](https://github.com/google/tcmalloc), 
[Facebook ZSTD](https://github.com/facebook/zstd).


### Install dependency on Ubuntu
#### Install glib, tcmalloc and cmake
```
sudo apt install libglib2.0-dev libgoogle-perftools-dev cmake
```

#### Install zstd
zstd must be installed from source
```
cd /tmp/
wget https://github.com/facebook/zstd/releases/download/v1.5.0/zstd-1.5.0.tar.gz
tar xvf zstd-1.5.0.tar.gz
cd zstd-1.5.0/build/cmake/
mkdir _build
cd _build/
cmake ..
make -j
sudo make install
```

#### Install XGBoost [Optional]
```
git clone --recursive https://github.com/dmlc/xgboost
pushd xgboost;
mkdir build && cd build;
cmake .. && make -j; 
sudo make install; 
popd
```

#### Install LightGBM [Optional]
```
git clone --recursive https://github.com/dmlc/xgboost
pushd xgboost;
mkdir build && cd build;
cmake .. && make -j; 
sudo make install; 
popd
```


### Install dependency on Mac
using [homebrew](https://brew.sh/) as an example
```
brew install cmake glib google-perftools
```

