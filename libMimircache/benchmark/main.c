//
// Created by Juncheng Yang on 4/23/20.
//

#include "include/cacheAlgoPerf.h"
#include "../include/mimircache.h"
#include "../cacheAlgo/include/cacheAlgoHeaders.h"

int main(int argc, char* argv[]){

  uint64_t cache_size = 1024*1024*1024;
  cache_t* fifo = FIFO_init(cache_size, OBJ_ID_NUM, NULL);
  cache_t* lru = LRU_init(cache_size, OBJ_ID_NUM, NULL);
  cache_t* slabLRC = slabLRC_init(cache_size, OBJ_ID_NUM, NULL);
  cache_t* slabLRU = slabLRU_init(cache_size, OBJ_ID_NUM, NULL);
  cache_t* slabObjLRU = slabObjLRU_init(cache_size, OBJ_ID_NUM, NULL);

  measure_qps(lru, 0.95);
  measure_qps(fifo, 0.95);
  measure_qps(slabLRC, 0.95);
  measure_qps(slabLRU, 0.95);
  measure_qps(slabObjLRU, 0.95);

  return 0;
}