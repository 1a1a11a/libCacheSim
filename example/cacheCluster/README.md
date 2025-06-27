# a cache cluster example
This illustrate how to build a cache cluster using consistent hashing.

## Build
Please install libCacheSim first.
Then you can run the following in this directory
```bash
# Prerequisites: Install Ninja build system if not already available
# Ubuntu/Debian: sudo apt install ninja-build
# macOS: brew install ninja

mkdir _build;
cd _build;
cmake -G Ninja ..;
ninja;
```


## Run
You can run the example trace
```bash
./cacheCluster
```
