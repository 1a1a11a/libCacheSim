

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




#include <libCacheSim.h>

#define million 1000000


/* gcc $(pkg-config --cflags --libs libCacheSim glib-2.0) -O2 -lm -ldl genL2.cpp -lstdc++ -o testcpp */


int main(int argc, char *argv[]) {
    char *ifile_path = argv[1];    
    char *ofile_path = argv[3]; 
    uint64_t cache_size = atoll(argv[2]); 
    time_t start_ts = time(NULL);

    /* output */
    FILE *ofile = fopen(ofile_path, "wb"); 


    /* open trace, see quickstart.md for opening csv and binary trace */
    reader_init_param_t init_parms;
    init_parms.obj_id_field = 1; 
    init_parms.obj_size_field=2; 
    strcpy(init_parms.binary_fmt, "QI"); 
    reader_t *reader = open_trace(argv[1], BIN_TRACE, OBJ_ID_NUM, &init_parms);
    reader_t *reader = open_trace(argv[1], TWR_TRACE, OBJ_ID_NUM, NULL); 


    /* craete a container for reading from trace */
    request_t *req = new_request();


    /* create a LRU cache */
    common_cache_params_t cc_params;
    cc_params.cache_size = cache_size; 
    cc_params.hashpower = 30; 
    cache_t *cache = create_cache("LRU", cc_params, NULL); 


    /* stat */
    uint64_t n_req = 0, n_req_L2 = 0; 
    std::unordered_set<obj_id_t> L2_obj; 
    uint64_t L2_obj_sizes = 0;

    /* loop through the trace */
    while (read_one_req(reader, req) == 0) {
        // if (req->obj_size > 2048)
        //     continue; 
        // if (req->obj_id % 25 != 0) 
        //     continue;
        n_req += 1;
        if (cache->get(cache, req) == cache_ck_miss) {
            /* write this request out */
            fwrite((void*) &(req->obj_id_int), 8, 1, ofile);
            fwrite((void*) &(req->obj_size), 4, 1, ofile);
            n_req_L2 += 1;

            if (L2_obj.find(req->obj_id_int) == L2_obj.end()) {
                L2_obj.insert(req->obj_id_int); 
                L2_obj_sizes += req->obj_size; 
            }
        }

        if (n_req % 100000000 == 0) {
            std::cout << std::setprecision(4) << n_req / million << ", " << 
                    n_req_L2 / million << ", " << (double) n_req_L2 / n_req 
                    << ", " << "L2 size " << (double) L2_obj_sizes / GB << "GB" << std::endl; 
        }
    }

    /* cleaning */
    close_trace(reader);
    free_request(req);
    cache->cache_free(cache);
    fclose(ofile); 
}

