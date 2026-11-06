# Install and build

This page gets libCacheSim built on your machine. You need a C/C++ compiler, [CMake](https://cmake.org/), [Ninja](https://ninja-build.org/), and three libraries:

| Library | Why it is needed | Required? |
| --- | --- | --- |
| [glib](https://docs.gtk.org/glib/) | hash tables and other data structures | yes |
| [zstd](https://github.com/facebook/zstd) | reading `.zst`-compressed traces | yes |
| [tcmalloc](https://github.com/google/tcmalloc) (gperftools) | a faster memory allocator | recommended |

If you only want to run simulations from Python, you can skip all of this and `pip install libcachesim` instead. See the [Python binding](https://github.com/cacheMon/libCacheSim-python).

## The quick way: use the install scripts

From the repository root:

```bash
cd scripts
bash ./install_dependency.sh
source ~/.bashrc          # or ~/.zshrc on macOS with zsh
bash ./install_libcachesim.sh
```

`install_dependency.sh` installs everything with `apt` on Ubuntu/Debian, `yum` on CentOS, or Homebrew on macOS. On Linux it also builds a recent CMake, zstd, and the optional machine-learning libraries XGBoost and LightGBM from source. That can take a while. If you don't need the learned algorithms (GLCache, LRB, 3LCache), the manual steps below are faster.

`install_libcachesim.sh` then builds libCacheSim into `_build/`.

If a script fails, please [open an issue](https://github.com/1a1a11a/libCacheSim/issues) with your OS and the error, and try the manual steps in the meantime.

## The manual way

### 1. Install the dependencies

**Ubuntu / Debian**

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build \
    libglib2.0-dev libgoogle-perftools-dev libzstd-dev
```

**macOS** (with [Homebrew](https://brew.sh/))

```bash
brew install cmake ninja glib gperftools zstd pkg-config argp-standalone
```

`argp-standalone` and `pkg-config` fix the two most common macOS build errors: `'argp.h' file not found` and `'glibconfig.h' file not found`. If the build later says zstd is not found, run `brew link zstd`.

**CentOS / RHEL**

```bash
sudo yum install glib2-devel google-perftools-devel ninja-build
```

On CentOS you will usually need to build zstd from source (next section).

### Building zstd from source (only if needed)

Recent distributions ship a zstd that works. If yours is old, or CMake stops with `zstd not found`, build it yourself:

```bash
wget https://github.com/facebook/zstd/releases/download/v1.5.0/zstd-1.5.0.tar.gz
tar xf zstd-1.5.0.tar.gz
cd zstd-1.5.0/build/cmake
cmake -G Ninja -B _build && ninja -C _build
sudo ninja -C _build install
```

### 2. Build libCacheSim

```bash
git clone https://github.com/1a1a11a/libCacheSim
cd libCacheSim
cmake -G Ninja -B _build
ninja -C _build
```

The default build type is `Release`, which is what you want for real experiments. The tools end up in `_build/bin/`: `cachesim`, `traceAnalyzer`, `mrcProfiler`, `tracePrint`, `traceConv`, `traceFilter`, and a few more.

Check that it works:

```bash
cd _build
./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb
cd ..
```

You should see a line ending in something like `miss ratio 0.6297, byte miss ratio 0.7274, throughput 4.62 MQPS`.

The rest of this page runs from the repository root, which is why the last line goes back up.

If CMake prints `!!! cannot find tcmalloc`, the build still works but runs slower. Install gperftools and re-run `cmake`.

### 3. Install the library (optional)

You only need this step if you want to write your own programs against libCacheSim (see the [library guide](advanced_lib.md)):

```bash
sudo ninja -C _build install
```

This installs the headers, the library, and a `libCacheSim.pc` file so `pkg-config --cflags --libs libCacheSim` works. On Linux, run `sudo ldconfig` afterwards if a program later fails with `error while loading shared libraries`.

### 4. Run the tests (optional)

```bash
ctest --test-dir _build --output-on-failure
```

## Optional algorithms

A few algorithms are off by default because they need extra libraries. Turn them on when you configure:

| Algorithm | CMake flag | Extra dependency |
| --- | --- | --- |
| GLCache | `-DENABLE_GLCACHE=ON` | [XGBoost](https://github.com/dmlc/xgboost) |
| LRB | `-DENABLE_LRB=ON` | [LightGBM](https://github.com/microsoft/LightGBM) |
| 3LCache | `-DENABLE_3L_CACHE=ON` | [LightGBM](https://github.com/microsoft/LightGBM) |
| S4-FIFO | `-DENABLE_S4FIFO=ON` | none |

For example:

```bash
cmake -G Ninja -B _build -DENABLE_GLCACHE=ON
ninja -C _build
```

If you ask `cachesim` for an algorithm that wasn't compiled in, it stops with `do not support algorithm <name>`.

To build XGBoost or LightGBM yourself (the install script does this for you):

```bash
git clone --recursive https://github.com/dmlc/xgboost     # or https://github.com/microsoft/LightGBM
cd xgboost                                                 # or LightGBM
cmake -G Ninja -B _build && ninja -C _build
sudo ninja -C _build install
```

Other build switches you may meet: `-DSUPPORT_TTL=ON` (honor object TTLs in traces), `-DUSE_HUGEPAGE=OFF` (see [performance tuning](performance.md)), and `-DCMAKE_BUILD_TYPE=Debug` (see the [debugging guide](debug.md)).
