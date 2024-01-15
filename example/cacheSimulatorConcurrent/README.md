
## Building a concurrent cache simulator 


This tutorial shows how to build a concurrent cache simulator using libCacheSim. 
The cache simulator can simulate multiple caches at the same time. 
This example illustrate the usage of `simulate_with_multi_caches()` and `simulate_at_multi_sizes()` APIs.
The details of the APIs can be found at [simulator.h](../../libCacheSim/include/libCacheSim/simulator.h)
This example also shows how to use a csv reader to read a csv trace.

### Build 
```bash
mkdir _build; 
cd _build;
cmake ..;
make;
```

### Run 
```bash
./cacheSimulatorMultiSize
```


