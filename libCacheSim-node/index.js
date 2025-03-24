const libcachesim = require('./build/Release/libcachesim');

// Initialize a 1MB LRU cache
const cache = libcachesim.initLRUCache(1024 * 1024);

console.log(libcachesim.cacheGet(cache, 1, 100));

libcachesim.freeCache(cache);