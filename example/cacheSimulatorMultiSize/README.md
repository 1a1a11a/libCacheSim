
## Building a concurrent cache simulator 
This tutorial shows how to build a concurrent cache simulator using libCacheSim. 
The concurrent cache simulator can simulate multiple caches at the same time and outputs the miss ratio at different sizes.
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
./cacheSimulatorMultiSize | tee result.txt
```

### Plot 
```bash
python3 ../plot.py --data result.txt --name LHD
```

