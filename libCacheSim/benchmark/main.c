//
// Created by Juncheng Yang on 4/23/20.
//

#include "../include/libCacheSim.h"
#include "../include/libCacheSim/evictionAlgo.h"
#include "include/cacheAlgoPerf.h"
#include "include/params.h"

void measure_all(common_cache_params_t cc_params);

int main(int argc, char* argv[]){
//  init_all_global_mem_alloc();
common_cache_params_t cc_params = {.cache_size=CACHE_SIZE, .default_ttl=0};
  printf("cache size %llu\n", (unsigned long long) CACHE_SIZE);

  if (argc < 2 || strcmp(argv[1], "all") == 0){
    measure_all(cc_params);
  } else if (argc == 2) {
    cache_t *cache = create_cache(argv[1], cc_params, NULL);
    measure_qps_write(cache);
    measure_qps_read(cache);
    cache->cache_free(cache);
  } else if (argc == 3 || argc == 4) {
    cache_t *cache = create_cache(argv[1], cc_params, NULL);
    reader_t *reader = setup_reader(argv[2], argv[3][0], OBJ_ID_NUM, NULL);
    measure_qps_withtrace(cache, reader);
  }

//  free_all_global_mem_alloc();
  return 0;
}


void measure_all(common_cache_params_t cc_params){

  cache_t* fifo = FIFO_init(cc_params, NULL);
  cache_t* lru = LRU_init(cc_params, NULL);
  cache_t* lruv0 = LRUv0_init(cc_params, NULL);
//  cache_t* slabLRC = slabLRC_init(cc_params, NULL);
//  cache_t* slabLRU = slabLRU_init(cc_params, NULL);
//  cache_t* slabObjLRU = slabObjLRU_init(cc_params, NULL);

  measure_qps_write(lru);
  measure_qps_read(lru);
  LRU_free(lru);

  measure_qps_write(fifo);
  measure_qps_read(fifo);
  FIFO_free(fifo);

//  measure_qps_write(slabLRC);
//  measure_qps_read(slabLRC);
//  slabLRC_free(slabLRC);

//  measure_qps_write(slabLRU);
//  measure_qps_read(slabLRU);
//  slabLRU_free(slabLRU);

//  measure_qps_write(slabObjLRU);
//  measure_qps_read(slabObjLRU);
//  slabObjLRU_free(slabObjLRU);
}