//
//  splay.h
//  LRUAnalyzer
//
//  Created by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//
//  Modified by Mingyan on 1/9/25.
//  For the use of SHARDS

#ifndef splay_tuple_h
#define splay_tuple_h

#ifdef __cplusplus
extern "C" {
#endif

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define node_value_t(x) (((x) == NULL) ? 0 : ((x)->value))
#define key_cmp_t(i, j) (int64_t)((int64_t)(i->Tmax) - (int64_t)(j->Tmax))

typedef struct key {
  size_t L;
  uint64_t Tmax;
} splay_key_t;

typedef splay_key_t* splay_key_type_t;

typedef struct sTree_tuple {
  struct sTree_tuple *left, *right;
  splay_key_type_t key;
  int64_t value;
} sTree_tuple;

static inline void free_node_t(sTree_tuple* t) { free(t); }
static inline void assign_key_t(sTree_tuple* t, splay_key_type_t k) {
  t->key = k;
}

sTree_tuple* splay_t(splay_key_type_t i, sTree_tuple* t);
sTree_tuple* insert_t(splay_key_type_t i, sTree_tuple* t);
sTree_tuple* splay_delete_t(splay_key_type_t i, sTree_tuple* t);
sTree_tuple* find_node_t(splay_key_type_t r, sTree_tuple* t);
void check_sTree_t(sTree_tuple* t);
void print_sTree_t(sTree_tuple* t, int d);
sTree_tuple* find_max_t(sTree_tuple* t);
void free_sTree_t(sTree_tuple* t);

#ifdef __cplusplus
}
#endif

#endif /* splay_h */
