/*
 * Refer to minimalIncrementCBF.h for documentation on the public interfaces.
 */

#include <assert.h>
#include <fcntl.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "minimalIncrementCBF.h"
#include "hash/hash.h"
#define XXH_INLINE_ALL
#include "hash/xxhash.h"
#include "hash/xxh3.h"


#define MAKESTRING(n) STRING(n)
#define STRING(n) #n

#define DEBUG_CBF
#undef DEBUG_CBF

int minimalIncrementCBF_init(struct minimalIncrementCBF * CBF, int entries, double error){
  if (CBF->ready == 1) {
    printf("CBF at %p already initialized!\n", (void *)CBF);
    return -1;
  }

  CBF->entries = entries;
  CBF->error = error;

  double num = log(error);
  double denom = 0.480453013918201; // ln(2)^2
  CBF->bpe = -num / denom;

  CBF->hashes = (int)ceil(0.693147180559945 * CBF->bpe); // ln(2)

  CBF->counter_num = (int)fmin(ceil(CBF->bpe * entries), INT_MAX);
  // if (CBF->counter_num < 1) {
  //   printf("CBF at %p has too few counters %d!\n", (void *)CBF, CBF->counter_num);
  //   return -1;
  // }
  if (CBF->counter_num < CBF->hashes) {
    printf("CBF at %p has too few counters %d! make it into 2 * %d\n",
      (void *)CBF, CBF->counter_num, CBF->hashes * 2);
    CBF->counter_num = CBF->hashes * 2;
  }

  CBF->bf = (unsigned int *)calloc(sizeof(unsigned int), CBF->counter_num);
  // TODO: check whether unsigned int is enough for the size of each counter

  if(CBF->bf == NULL) {
// #ifdef DEBUG_CBF
    printf("CBF at %p failed to allocate memory (aimed to allocate %d counters!)\n", (void *)CBF, CBF->counter_num);
    printf("Note it is possible that the counter size should be divided by average object size.\n");
// #endif
    exit(1);
  }
  CBF->ready = 1;
  return 0;
}

int minimalIncrementCBF_init_size(struct minimalIncrementCBF * CBF, int entries, double error,
                    unsigned int cache_size) {
  minimalIncrementCBF_init(CBF, entries, error);
  return 0;
}

static int minimalIncrementCBF_check_add(struct minimalIncrementCBF * CBF,
                            const void * buffer, int len, int add)
{
  if (CBF->ready == 0) {
    printf("CBF at %p not initialized!\n", (void *)CBF);
    return -1;
  }

  if(add != 0 && add != 1) {
    printf("add should be 0 or 1!\n");
    return -1;
  }

  // int hits = 0;
  register unsigned int a = XXH64(buffer, len, HASH_SEED0);
  register unsigned int b = XXH64(buffer, len, HASH_SEED1);
  register unsigned int x;
  register unsigned int i;

  //register unsigned int counter_num = CBF->counter_num;
  register unsigned int min_count = UINT_MAX;
  
  assert(CBF->counter_num != 0);
  for (i = 0; i < CBF->hashes; i++) {
    x = (a + i * b) % CBF->counter_num;
    if (CBF->bf[x] < min_count) {
#ifdef DEBUG_CBF
      printf("[CBF search] x = %u, bf[x] = %u < min_count = %u\n", x, CBF->bf[x], min_count);
#endif
      min_count = CBF->bf[x];
    }
    else {
#ifdef DEBUG_CBF
      printf("[CBF search] x = %u, bf[x] = %u >= min_count = %u\n", x, CBF->bf[x], min_count);
#endif
    }
  }

  if (add == 1 && min_count < UINT_MAX) {
    for (i = 0; i < CBF->hashes; i++) {
      x = (a + i * b) % CBF->counter_num;
      if(CBF->bf[x] == min_count) {
#ifdef DEBUG_CBF
        printf("[CBF add before] x = %u, bf[x] = %u, min_count = %u\n", x, CBF->bf[x], min_count);
#endif
      }
        CBF->bf[x]++;
    }
    min_count++;
  }

  return min_count;
}

int minimalIncrementCBF_estimate(struct minimalIncrementCBF * CBF, const void * buffer, int len) {
  return minimalIncrementCBF_check_add(CBF, buffer, len, 0);
}

int minimalIncrementCBF_add(struct minimalIncrementCBF * CBF, const void * buffer, int len) {
  return minimalIncrementCBF_check_add(CBF, buffer, len, 1);
}

void minimalIncrementCBF_print(struct minimalIncrementCBF * CBF) {
  printf("Minimal Increment CBF:\n");
  printf("  entries: %d\n", CBF->entries);
  printf("  error: %f\n", CBF->error);
  printf("  counters per elem: %f\n", CBF->bpe);
  printf("  counters: %d\n", CBF->counter_num);
  printf("  hash functions = %d\n", CBF->hashes);
}

void minimalIncrementCBF_free(struct minimalIncrementCBF * CBF) {
  if (CBF->ready) {
    free(CBF->bf);
  }
  CBF->ready = 0;
}

int minimalIncrementCBF_decay(struct minimalIncrementCBF * CBF) {
#ifdef DEBUG_CBF
  printf("call decay\n");
#endif
  if (!CBF->ready) {
    return 1;
  }
  assert(CBF->bf != NULL);
  for (int i = 0; i < CBF->counter_num; i++) {
#ifdef DEBUG_CBF
    printf("[CBF decay] bf[%d] = %u\n", i, CBF->bf[i]);
#endif
    CBF->bf[i] = CBF->bf[i] >> 1;
  }

  return 0;
}

const char * minimalIncrementCBF_version(void) {
  return MAKESTRING(VERSION);
}