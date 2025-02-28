#include "../../include/libCacheSim/reader.h"
#include "../../include/libCacheSim/sampling.h"
#include "../../utils/include/mymath.h"
#include "../../utils/include/mystr.h"
#include "../../utils/include/mysys.h"
#include "DataStructure/histogram.h"
#include "DataStructure/splay.h"
#include "DataStructure/splay_tuple.h"
#include "mrc_internal.h"

#ifdef __cplusplus
extern "C" {
#endif

void generate_shards_mrc(struct PARAM *params, char *path) {
  srand(time(NULL));
  set_rand_seed(rand());
  reader_t *reader = params->reader;
  request_t *req = new_request();
  size_t timestamp = 0;

  uint64_t n_req = get_num_of_req(reader);
  printf("n_req: %lu\n", n_req);
  params->data = init_histogram();
  params->prio_tree = NULL;
  params->distance_tree = NULL;
  params->lookup_hash = g_hash_table_new(g_direct_hash, g_direct_equal);
  uint64_t maxmax = 0;
  // reset_reader(reader);
  read_one_req(reader, req);
  uint64_t req_cnt = 0;
  // FILE *f = fopen("/users/Claire/libCacheSim/debug_shards.log", "w");
  // FILE *shards_log = fopen("/users/Claire/libCacheSim/debug_shards_evictions.log", "w");
  while (req->valid) {
    req_cnt++;
    // if (req_cnt < 10000) {
    //   fprintf(f, "req_cnt:%lu, obj_id: %lu\n", req_cnt, req->obj_id);
    // }
    // print_request(req, INFO_LEVEL);
    if (g_hash_table_lookup(params->lookup_hash, GSIZE_TO_POINTER((gsize)req->obj_id)) == NULL) {
      g_hash_table_insert(params->lookup_hash, GSIZE_TO_POINTER((gsize)req->obj_id),
                          GSIZE_TO_POINTER((gsize)timestamp));
      params->distance_tree = insert((long long)timestamp,req->obj_size, params->distance_tree);

      if (params->ver == 1) {
        struct key *new_tuple = (struct key *)malloc(sizeof(struct key));
        new_tuple->L = req->obj_id;
        new_tuple->Tmax = (req->hv) & ((1 << 24) - 1);
        if (new_tuple->Tmax > maxmax) {
          maxmax = new_tuple->Tmax;
          printf("maxmax: %lu\n", maxmax);
        }
        params->prio_tree = insert_t(new_tuple, params->prio_tree);
      }

      while ((params->ver == 1) && (params->prio_tree->value >= params->threshold)) {
        struct key *max = find_max_t(params->prio_tree)->key;
        uint64_t last_max = 0;
        while ((last_max == max->Tmax) || (last_max == 0)) {
          obj_id_t id = max->L;
          last_max = max->Tmax;
          params->rate = (double)max->Tmax / (double)(1 << 24);
          printf("rate: %.25f\n", params->rate);
          reader->sampler->sampling_ratio = params->rate;
          params->prio_tree = splay_delete_t(max, params->prio_tree);
          gpointer hash_value = g_hash_table_lookup(params->lookup_hash, GSIZE_TO_POINTER((gsize)id));
          g_hash_table_remove(params->lookup_hash, GSIZE_TO_POINTER((gsize)id));
          params->distance_tree = splay_delete((long long)hash_value, params->distance_tree);
          max = find_max_t(params->prio_tree)->key;
        }
      }
      update_histogram(params->data, (uint64_t)-1, params->rate);
    } else {
      gpointer hash_value = g_hash_table_lookup(params->lookup_hash, GSIZE_TO_POINTER((gsize)req->obj_id));
      params->distance_tree = splay_delete((long long)hash_value, params->distance_tree);
      long long distance =
          find_range((long long)hash_value, params->distance_tree);  // timestamp - *(uint64_t *)hash_value;

      // Open log file in append mode

      // if (shards_log) {
      //   if (distance > 1000) {
      //     fprintf(shards_log, "Evicted Obj ID: %lu | Reuse Distance: %lld | Cache Size Threshold: 1000\n", req->obj_id,
      //             distance);
      //   }
      // }

      g_hash_table_replace(params->lookup_hash, GSIZE_TO_POINTER((gsize)req->obj_id),
                           GSIZE_TO_POINTER((gsize)timestamp));
      params->distance_tree = insert(timestamp, req->obj_size,params->distance_tree);
      update_histogram(params->data, distance, params->rate);
    }

    read_one_req(reader, req);
    timestamp++;
  }
  // histogram 重新遍历
  if (params->ver == 1) {
    wrap_up_histogram(params->data, params->rate);
  }
  
  reader->n_total_req = 0;
  reader->init_params.sampler->sampling_ratio = 1.0;
  uint64_t expected_reqs = n_req * params->rate;
  printf("total_req: %lu\n", n_req);
  printf("expected_reqs: %lu\n", expected_reqs);
  printf("bins[0]: %lu\n", get_min_distance(params->data));
  if (expected_reqs > n_req) {
    for (int i=0;i<expected_reqs - n_req;i++) {
      update_histogram(params->data, get_min_distance(params->data), params->rate);
    }
  }
  printf("bins[0]: %lu\n", get_min_distance(params->data));

  export_histogram_to_csv(params->data, params->rate, path);
  g_hash_table_destroy(params->lookup_hash);
  free_request(req);
  free_sTree_t(params->prio_tree);
  free_sTree(params->distance_tree);
  // fclose(shards_log);
  close_reader(reader);
  // fclose(f);
}

#ifdef __cplusplus
}
#endif