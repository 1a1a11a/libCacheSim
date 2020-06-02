//
// Created by Juncheng Yang on 6/1/20.
//

#ifndef LIBMIMIRCACHE_CUCKOO_H
#define LIBMIMIRCACHE_CUCKOO_H


#include <inttypes.h>
#include <stdbool.h>
#include "hashtable.h"

#ifdef __cplusplus
extern "C"
{
#endif

  typedef struct {
    int n_hash;
    int n_displace;
  } cuckoo_ht_params_t;


hashtable_t* cuckoo_setup(uint64_t n_items);

void cuckoo_teardown(hashtable_t* ht);

void cuckoo_reset(hashtable_t* ht);

struct item *cuckoo_get(void* obj);

struct item *cuckoo_insert(struct bstring *key, struct val *val, proc_time_i expire);

rstatus_i cuckoo_update(struct item *it, struct val *val, proc_time_i expire);

bool cuckoo_delete(struct bstring *key);


#ifdef __cplusplus
}
#endif




#endif //LIBMIMIRCACHE_CUCKOO_H
