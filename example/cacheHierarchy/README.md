# a cache hierarchy example
This simulates several L1 caches (each with one trace) and one L2 cache by first generating the misses of the L1 caches and feed in the L2 cache.
It outputs the L2 miss ratio curve.


## Dependency
* libCacheSim: you must install libCacheSim first


## Build
```bash
# Prerequisites: Install Ninja build system if not already available
# Ubuntu/Debian: sudo apt install ninja-build
# macOS: brew install ninja

mkdir _build/;
cd _build/;
cmake -G Ninja ../;
ninja;

```


## Run
./layeredCache ../config.yaml
