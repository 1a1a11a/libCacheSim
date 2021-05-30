//
// Created by Juncheng Yang on 4/23/20.
//

#include "../include/libCacheSim.h"
#include "../include/libCacheSim/evictionAlgo.h"
#include "include/cacheAlgoPerf.h"
#include "include/params.h"

void measure_all(common_cache_params_t cc_params);

int main(int argc, char *argv[]) {
  srand(time(NULL));

  common_cache_params_t cc_params = {.cache_size=CACHE_SIZE, .default_ttl=0};

//  cache_t *cache = LRU_init(cc_params, NULL);
//  reader_t *reader = open_trace("../data/trace.vscsi", VSCSI_TRACE, OBJ_ID_NUM, NULL);
//  measure_qps_withtrace(cache, reader);
//  cache->cache_free(cache);
//  close_trace(reader);


  measure_all(cc_params);

  return 0;
}

void measure_all(common_cache_params_t cc_params) {

  cache_t *fifo = FIFO_init(cc_params, NULL);
  cache_t *lru = LRU_init(cc_params, NULL);

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