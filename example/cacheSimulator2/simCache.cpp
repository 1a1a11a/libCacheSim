

#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctime>

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <iostream>
#include <iomanip>
#include <thread>



#include <libCacheSim.h>

#define million 1000000


/* gcc $(pkg-config --cflags --libs libCacheSim glib-2.0) -O2 -lm -ldl simCache.cpp -lstdc++ -o test */


#define N_SIM_PTS 2000

int main(int argc, char *argv[]) {
    if (argc != 5) {
        printf("usage: %s input_path output_path size algo\n", argv[0]); 
        exit(1); 
    }

    uint64_t cache_size = atoll(argv[3]); 
    uint64_t cache_sizes[N_SIM_PTS]; 
    for (int i=0; i<N_SIM_PTS; i++) 
        cache_sizes[i] = cache_size * (i+1) / N_SIM_PTS; 

    reader_t *reader = open_trace(argv[1], PLAIN_TXT_TRACE, OBJ_ID_NUM, NULL); 

    common_cache_params_t cc_params;
    cc_params.cache_size = cache_size; 
    cc_params.hashpower = 20; 
    cache_t *cache = create_cache(argv[4], cc_params, NULL); 

    /* see libCacheSim/include/simulator.h for sim_res_t */
    sim_res_t *result = simulate_at_multi_sizes(reader, cache, N_SIM_PTS, cache_sizes, NULL, 0, static_cast<int>(std::thread::hardware_concurrency())); 
    
    FILE *ofile = fopen(argv[2], "wb"); 
    fprintf(ofile, "size hit\n"); 
    for (int i=0; i<N_SIM_PTS; i++) {
        fprintf(ofile, "%lu %lu %lu %.4lf\n", cache_size * (i+1) / N_SIM_PTS, 
                result[i].req_cnt, result[i].req_cnt - result[i].miss_cnt, 
                (double) (result[i].req_cnt - result[i].miss_cnt) / result[i].req_cnt); 
    }


    close_trace(reader);
    cache->cache_free(cache);
    fclose(ofile); 
}






