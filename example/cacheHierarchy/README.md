# a cache hierarchy example
This simulates several L1 caches (each with one trace) and one L2 cache by first generating the misses of the L1 caches and feed in the L2 cache. 
It outputs the L2 miss ratio curve. 


## Dependency
* libCacheSim: you must install libCacheSim first


## Build
```bash
mkdir _build/;
cd _build/;
cmake ../;
make -j;

```


## Run 
./layeredCache ../config.yaml

