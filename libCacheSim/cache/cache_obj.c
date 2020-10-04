////
//// Created by Juncheng Yang on 10/3/20.
////
//
//#include "../include/libCacheSim/cache_obj.h"
//#include "../include/libCacheSim/struct.h"
//#include "../include/libCacheSim/mem.h"
//#include "../include/libCacheSim/const.h"
//#include "../include/libCacheSim/logging.h"
//
//#define MAX_DIM1 1<<20
//#define MAX_DIM2 1<<20
//static cache_obj_t *all_objs[MAX_DIM2] = {NULL};
//static uint64_t obj_idx1 = 0;
//static uint64_t obj_idx2 = 0;
//static cache_obj_t *free_obj_q = NULL;
//
//cache_obj_t *new_cache_obj(uint32_t *obj_idx1, uint32_t *obj_idx2) {
//  cache_obj_t *cache_obj;
//  if (free_obj_q != NULL) {
//    cache_obj = free_obj_q;
//    free_obj_q = cache_obj->list_next;
//  } else {
//    if (obj_idx2 == 0) {
//      all_objs[obj_idx1] = malloc(sizeof(cache_obj_t) * MAX_DIM1);
//    }
//    cache_obj = &all_objs[obj_idx1][obj_idx2++];
//    if (obj_idx2 == MAX_DIM2) {
//      obj_idx2 = 0;
//      obj_idx1 ++;
//    }
//  }
//
//  return cache_obj;
//}
//
