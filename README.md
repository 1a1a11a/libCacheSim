libMimircache - a library supporting caching benchmark and caching trace profiling 

changing from trace as first-class citizen to cache as first class citizen 
external cache replacement algorithms 

name refactoring
dynamic compiling
add tests
add size support


goal: design libMimircache as a library, not a binary
separate LRUProfiler and simulator

rename cacheline
allow a cache to run through a trace so that user can run multiple traces
remove TRACK_ACCESS_TIME in LRUSize
remove cache->core.ts
remove STACK_DIST and bp from reader (but then how do you pass to python)



Handling storage block cache traces
For the simplicity of most users, libmimircache decides to drop native support for block traces using logical block number/address (LBN/LBA), instead a user is expected to convert the trace into a stardard format,
in which, obj_id is LBA/block_size while obj_size is request_size/block_size. We have provided some tools to help with this conversion, see tools/convert_storage_trace.sh

for efficiency, we stopped supporting using 64-bit obj_id on 32-bit platform 


#### Features
* high performance - over 20M requests/sec for a realistic trace replay
* low memory footprint bounded memory usage 
* 
* native support 



#### Build
cmake recommends out-of-source build, so we do it in a new directory:
```
mkdir _build
cd _build
cmake ..
make -j
make test
```

#### Usage


#### Reference


#### Contributions 


How it works


