## Building a cache simulator 
This tutorial shows how to build a cache simulator using libCacheSim. 
There are three data structures required to build a cache simulator:
1. create a trace reader using `open_trace()`
2. create a cache using `LRU_init()` where LRU can be replaced by other cache replacement policies
3. create a request handler using `new_request()`

With a reader, a cache and a request data structure, we can simulate the cache by repeated reading from the reader and feed into the cache.
```c
while (read_one_req(reader, req) == 0) {
    if (cache->get(cache, req)) {
        // cache hit
    } else {
        // cache miss
    }
}
```
See [main.c](main.c) for a complete example.

### Build 
```bash
mkdir _build; 
cd _build;
cmake ..;
make;
```

### Run 
```bash
./cacheSimulator
```

### Faq
1. I have an error `./cacheSimulator: error while loading shared libraries: liblibCacheSim.so.0.1.0: cannot open shared object file: No such file or directory`
Solution
* run `sudo ldconfig`
* check whether the library is under /usr/local/lib
* 

 