// test.cc
#include <iostream>
#include "../libCacheSim/include/libCacheSim.h"


int main() {
    std::cout << "Trying to include libCacheSim..." << std::endl;

    // Set up basic cache parameters
    common_cache_params_t params = {
        .cache_size = 1024 * 1024, // 1 MB cache
        .default_ttl = 0,
        .hashpower = 24
    };

    // Try to create an LRU cache
    cache_t* cache = LRU_init(params, nullptr);

    if (cache) {
        std::cout << "Successfully created an LRU cache." << std::endl;
        cache->cache_free(cache);  // Proper cleanup
    } else {
        std::cerr << "Failed to create cache." << std::endl;
        return 1;
    }

    return 0;
}
