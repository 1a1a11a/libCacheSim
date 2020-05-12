//
// Created by Juncheng Yang on 4/23/20.
//

#include "include/cacheAlgoPerf.h"
#include "../include/mimircache.h"
#include "../cacheAlgo/include/cacheAlgoHeaders.h"

int main(int argc, char* argv[]){

  uint64_t cache_size = 1024*1024*1024;
  common_cache_params_t cc_params = {.cache_size=cache_size, .obj_id_type=OBJ_ID_NUM, .support_ttl=FALSE};
  cache_t* fifo = FIFO_init(cc_params, NULL);
  cache_t* lru = LRU_init(cc_params, NULL);
  cache_t* slabLRC = slabLRC_init(cc_params, NULL);
  cache_t* slabLRU = slabLRU_init(cc_params, NULL);
  cache_t* slabObjLRU = slabObjLRU_init(cc_params, NULL);

  measure_qps_write(lru, 0.95);
  measure_qps_write(fifo, 0.95);
  measure_qps_write(slabLRC, 0.95);
  measure_qps_write(slabLRU, 0.95);
  measure_qps_write(slabObjLRU, 0.95);

  measure_qps_read(lru, 0.95);
  measure_qps_read(fifo, 0.95);
  measure_qps_read(slabLRC, 0.95);
  measure_qps_read(slabLRU, 0.95);
  measure_qps_read(slabObjLRU, 0.95);

  return 0;
}