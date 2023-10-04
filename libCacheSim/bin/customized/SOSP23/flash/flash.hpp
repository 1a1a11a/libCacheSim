
#include "../../../../include/libCacheSim/cache.h"
#include "../../../../include/libCacheSim/reader.h"
#include "../../../../utils/include/mymath.h"
#include "../../../../utils/include/mystr.h"
#include "../../../../utils/include/mysys.h"
#include "../../../cachesim/internal.h"

typedef struct {
  cache_t *ram;
  cache_t *disk;

  int64_t n_obj_admit_to_ram;
  int64_t n_obj_admit_to_disk;
  int64_t n_byte_admit_to_ram;
  int64_t n_byte_admit_to_disk;

  double ram_size_ratio;
  double disk_admit_prob;
  int inv_prob;

  char ram_cache_type[32];
  request_t *req_local;
} flashProb_params_t;

typedef enum {
  RETAIN_POLICY_RECENCY = 0,
  RETAIN_POLICY_FREQUENCY,
  RETAIN_POLICY_BELADY,
  RETAIN_NONE
} retain_policy_t;

typedef struct FIFO_Reinsertion_params {
  cache_obj_t *q_head;
  cache_obj_t *q_tail;

  // points to the eviction position
  cache_obj_t *next_to_merge;
  // the number of object to examine at each eviction
  int n_exam_obj;
  // of the n_exam_obj, we keep n_keep_obj and evict the rest
  int n_keep_obj;
  // used to sort the n_exam_obj objects
  struct sort_list_node *metric_list;
  // the policy to determine the n_keep_obj objects
  retain_policy_t retain_policy;

  int64_t n_obj_rewritten;
  int64_t n_byte_rewritten;
} FIFO_Reinsertion_params_t;

typedef struct {
  cache_t *fifo;
  cache_t *fifo_ghost;
  cache_t *main_cache;
  bool hit_on_ghost;

  int64_t n_obj_admit_to_fifo;
  int64_t n_obj_admit_to_main;
  int64_t n_obj_move_to_main;
  int64_t n_obj_rewritten_in_main;
  int64_t n_byte_admit_to_fifo;
  int64_t n_byte_admit_to_main;
  int64_t n_byte_move_to_main;
  int64_t n_byte_rewritten_in_main;

  double fifo_size_ratio;
  double ghost_size_ratio;
  char main_cache_type[32];

  request_t *req_local;
} QDLPv1_params_t;

typedef struct WTinyLFU_params {
  cache_t *LRU;         // LRU as windowed LRU
  cache_t *main_cache;  // any eviction policy
  double window_size;
  int64_t n_admit_bytes;
  struct minimalIncrementCBF *CBF;
  size_t max_request_num;
  size_t request_counter;
  char main_cache_type[32];

  request_t *req_local;
} WTinyLFU_params_t;
