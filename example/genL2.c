

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>

#include <libCacheSim.h>


int main(int argc, char *argv[]) {
    char *ifile_path = argv[1];    
    char *ofile_path = argv[3]; 
    uint64_t cache_size = atoll(argv[2]); 

    /* output */
    FILE *ofile = fopen(ofile_path, "wb"); 


    reader_init_param_t init_parms;
    init_parms.obj_id_field = 1; 
    init_parms.obj_size_field=2; 
    strcpy(init_parms.binary_fmt, "QI"); 


    /* open trace, see quickstart.md for opening csv and binary trace */
    reader_t *reader = open_trace(argv[1], BIN_TRACE, OBJ_ID_NUM, &init_parms);

    /* craete a container for reading from trace */
    request_t *req = new_request();

    /* create a LRU cache */
    common_cache_params_t cc_params = {.cache_size=cache_size}; 
    cc_params.hashpower = 24;
    cache_t *cache = create_cache("LRU", cc_params, NULL); 

    uint64_t n_req = 0, n_req_L2 = 0; 
    /* loop through the trace */
    while (read_one_req(reader, req) == 0) {
        // printf("req %llu %llu\n", req->obj_id, req->obj_size);
        n_req += 1;
        if (cache->get(cache, req) == cache_ck_miss) {
            /* write this request out */
            fwrite((void*) &(req->obj_id), 8, 1, ofile);
            fwrite((void*) &(req->obj_size), 4, 1, ofile);
            n_req_L2 += 1;
        }

        if (n_req % 100000000 == 0) 
            printf("req %"PRIu64" M %"PRIu64 " M - %.4lf filtered\n", 
                   n_req/1000000, n_req_L2/1000000, 1 - (double) n_req_L2/n_req); 
    }

    /* cleaning */
    close_trace(reader);
    free_request(req);
    cache->cache_free(cache);
    fclose(ofile); 
}


