#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "../../dataStructure/histogram.h"
#include "../../dataStructure/splay.h"
#include "../../dataStructure/splay_tuple.h"  ///users/Claire/libCacheSim/libCacheSim/profiler/dist.c
#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/sampling.h"
#include "../../profiler/dist.c"  // for get_stack_dist_add_req, etc.
#include "../../utils/include/mymath.h"
#include "../../utils/include/mystr.h"
#include "../../utils/include/mysys.h"
#include "mrc_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

uint64_t simulate_shards_mrc(struct PARAM *params);

// Compute reuse distance for each request in fixed-rate mode.
int64_t compute_distance_fixed_rate(struct PARAM *params, request_t *req, uint64_t timestamp) {
  int64_t last_access = -1;
  int64_t distance =
      get_stack_dist_add_req(req, &params->distance_tree, params->lookup_hash, (int64_t)timestamp, &last_access);
  return distance;
}

// Compute reuse distance for each request in fixed-size mode.
int64_t compute_distance_fixed_size(struct PARAM *params, request_t *req, uint64_t timestamp) {
  int64_t last_access = -1;
  int64_t distance =
      get_stack_dist_add_req(req, &params->distance_tree, params->lookup_hash, (int64_t)timestamp, &last_access);

  // If the object has not been accessed before, insert it into the priority tree.
  if (distance == -1) {
    struct key *new_tuple = malloc(sizeof(struct key));
    new_tuple->L = req->obj_id;
    new_tuple->Tmax = (req->hv) & ((1 << 24) - 1);
    params->prio_tree = insert_t(new_tuple, params->prio_tree);
  }
  
  // Update the priority tree and lookup hash when number of stored objects exceeds the threshold.
  while (params->prio_tree != NULL && params->prio_tree->value >= params->threshold) {
    struct key *max = find_max_t(params->prio_tree)->key;
    uint64_t last_max = 0;
    params->rate = (double)max->Tmax / (double)(1 << 24);
    //printf("rate: %f\n", params->rate);
    // Update the sampler ratio when stored object overflows
    params->reader->sampler->sampling_ratio = params->rate;
    while ((last_max == max->Tmax) || (last_max == 0)) {
      obj_id_t id = max->L;
      if (id==req->obj_id) distance = -2;
      last_max = max->Tmax;
      // Remove the key from prio_tree and update lookup and distance_tree.
      params->prio_tree = splay_delete_t(max, params->prio_tree);
      gpointer hash_value_inner = g_hash_table_lookup(params->lookup_hash, GSIZE_TO_POINTER((gsize)id));
      g_hash_table_remove(params->lookup_hash, GSIZE_TO_POINTER((gsize)id));
      params->distance_tree = splay_delete((long long)hash_value_inner, params->distance_tree);
      if (params->prio_tree)
        max = find_max_t(params->prio_tree)->key;
      else
        break;
    }
  }

  return distance;
}

uint64_t simulate_shards_mrc(struct PARAM *params) {
  reader_t *reader = params->reader;
  request_t *req = new_request();
  uint64_t timestamp = 0;
  uint64_t n_req=0;
  read_one_req(reader, req);
  while (req->valid) {
    int64_t distance = params->compute_distance(params, req, timestamp);
    if (distance == -2) {
      read_one_req(reader, req);
      timestamp++;
      n_req++;
      continue;
    }
    update_histogram(params->data, distance, params->rate);  
    read_one_req(reader, req);
    timestamp++;
    n_req++;
  }
  free_request(req);
  return n_req;
}

void generate_shards_mrc(struct PARAM *params, char *path) {
  srand(time(NULL));
  set_rand_seed(rand());

  // Calculate the number of requests in the original trace.
  params->reader->init_params.sampler->sampling_ratio = 1.0;
  params->reader->sampler->sampling_ratio = 1.0;
  uint64_t n_req = get_num_of_req(params->reader);
  printf("n_req: %lu\n", n_req);
  params->reader->init_params.sampler->sampling_ratio = params->rate;
  params->reader->sampler->sampling_ratio = params->rate;

  // Initialize the data structures.
  params->data = init_histogram();
  params->prio_tree = NULL;
  params->distance_tree = NULL;
  params->lookup_hash = g_hash_table_new(g_direct_hash, g_direct_equal);

  // Start the simulation.
  uint64_t read_req=simulate_shards_mrc(params);

  // In fixed-size mode, perform additional post-processing.
  if (params->ver == true) {
    wrap_up_histogram(params->data, params->rate);
  }

  //SHARDS-adj
  adjust_histogram(params->data, n_req, params->rate);
  
  export_histogram_to_csv(params->data, params->rate, path);
  g_hash_table_destroy(params->lookup_hash);
  free_sTree_t(params->prio_tree);
  free_sTree(params->distance_tree);
  close_reader(params->reader);
}

#ifdef __cplusplus
}
#endif
