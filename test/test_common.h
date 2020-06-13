//
// Created by Juncheng Yang on 11/21/19.
//

#ifndef MIMIRCACHE_TEST_COMMON_H
#define MIMIRCACHE_TEST_COMMON_H


#include <glib.h>
#include <glib/gi18n.h>
#include "../libMimircache/include/mimircache.h"
#include "../libMimircache/cacheAlgo/include/cacheAlgoHeaders.h"


#define BLOCK_UNIT_SIZE 0    // 16 * 1024
#define DISK_SECTOR_SIZE 0  // 512

#define N_TEST_REQ 6
#define N_TEST 6

#define CACHE_SIZE_UNIT (MiB)
//#define CACHE_SIZE_UNIT (GiB)
#define CACHE_SIZE (1024 * CACHE_SIZE_UNIT)
#define STEP_SIZE (128 * CACHE_SIZE_UNIT)
//#define CACHE_SIZE (1 * CACHE_SIZE_UNIT)
//#define STEP_SIZE (1 * CACHE_SIZE_UNIT)


#define NUM_OF_THREADS 1

reader_t *setup_vscsi_reader() {
  reader_t *reader_vscsi = setup_reader("../../data/trace.vscsi", VSCSI_TRACE, OBJ_ID_NUM, NULL);
  return reader_vscsi;
}

reader_t *setup_binary_reader() {
  reader_init_param_t *init_params_bin = g_new0(reader_init_param_t, 1);
  strcpy(init_params_bin->binary_fmt, "<3I2H2Q");
  init_params_bin->obj_size_field = 2;
  init_params_bin->obj_id_field = 6;
  init_params_bin->real_time_field = 7;
  reader_t *reader_bin_l = setup_reader("../../data/trace.vscsi", BIN_TRACE, OBJ_ID_NUM, init_params_bin);
  g_free(init_params_bin);
  return reader_bin_l;
}

reader_t *setup_csv_reader_obj_str() {
  reader_init_param_t *init_params_csv = g_new0(reader_init_param_t, 1);
  init_params_csv->delimiter = ',';
  init_params_csv->real_time_field = 2;
  init_params_csv->obj_id_field = 5;
  init_params_csv->obj_size_field = 4;
  init_params_csv->has_header = TRUE;
  reader_t *reader_csv_c = setup_reader("../../data/trace.csv", CSV_TRACE, OBJ_ID_STR, init_params_csv);
  g_free(init_params_csv);
  return reader_csv_c;
}

reader_t *setup_csv_reader_obj_num() {
  reader_init_param_t *init_params_csv = g_new0(reader_init_param_t, 1);
  init_params_csv->delimiter = ',';
  init_params_csv->real_time_field = 2;
  init_params_csv->obj_id_field = 5;
  init_params_csv->obj_size_field = 4;
  init_params_csv->has_header = TRUE;
  reader_t *reader_csv_l = setup_reader("../../data/trace.csv", CSV_TRACE, OBJ_ID_NUM, init_params_csv);
  g_free(init_params_csv);
  return reader_csv_l;
}

reader_t *setup_plaintxt_reader_num() {
  return setup_reader("../../data/trace.txt", PLAIN_TXT_TRACE, OBJ_ID_NUM, NULL);
}

reader_t *setup_plaintxt_reader_str() {
  return setup_reader("../../data/trace.txt", PLAIN_TXT_TRACE, OBJ_ID_STR, NULL);
}


void test_teardown(gpointer data) {
  reader_t *reader = (reader_t *) data;
  close_reader(reader);
}

cache_t *create_test_cache(const char *alg_name, common_cache_params_t cc_params, reader_t* reader, void *params) {
  cache_t* cache;
  void *init_params_g;
  if (strcmp(alg_name, "LRU") == 0)
    cache = LRU_init(cc_params, NULL);
  else if (strcmp(alg_name, "FIFO") == 0)
    cache = FIFO_init(cc_params, NULL);
  else if (strcmp(alg_name, "LRUv0") == 0)
    cache = LRUv0_init(cc_params, NULL);
  else if (strcmp(alg_name, "Random") == 0)
    cache = Random_init(cc_params, NULL);
  else if (strcmp(alg_name, "MRU") == 0)
    cache = MRU_init(cc_params, NULL);
//  else if (strcmp(alg_name, "LRU_K") == 0)
//    cache = LRU_K_init(cc_params, NULL);
//  else if (strcmp(alg_name, "LFU") == 0)
//    cache = LFU_init(cc_params, NULL);
//  else if (strcmp(alg_name, "LFUFast") == 0)
//    cache = LFUFast_init(cc_params, NULL);
//  else if (strcmp(alg_name, "ARC") == 0) {
//    ARC_init_params_t *init_params = g_new0(ARC_init_params_t, 1);
//    init_params_g = init_params;
//    init_params->ghost_list_factor = 100;
//    cache = ARC_init(cc_params, init_params);
//  } else if (strcmp(alg_name, "SLRU") == 0) {
//    SLRU_init_params_t *init_params = g_new0(SLRU_init_params_t, 1);
//    init_params_g = init_params;
//    init_params->n_seg = 2;
//    cache = SLRU_init(cc_params, init_params);
//  } else if (strcmp(alg_name, "Optimal") == 0) {
//    struct Optimal_init_params *init_params = g_new0(struct Optimal_init_params, 1);
//    init_params_g = init_params;
//    init_params->reader = reader;
//    init_params->ts = 0;
//    cache = Optimal_init(cc_params, (void *) init_params);
//  } else if (strcmp(alg_name, "TTL_FIFO") == 0){
//    cache = TTL_FIFO_init(cc_params, NULL);
//  }
  else if (strcmp(alg_name, "slabLRC") == 0) {
    cache = slabLRC_init(cc_params, NULL);
  } else if (strcmp(alg_name, "slabLRU") == 0) {
    cache = slabLRU_init(cc_params, NULL);
  } else if (strcmp(alg_name, "slabObjLRU") == 0) {
    cache = slabObjLRU_init(cc_params, NULL);
//
//  } else if (strcmp(alg_name, "PG") == 0) {
//    PG_init_params_t *init_params = g_new0(PG_init_params_t, 1);
//    init_params_g = init_params;
//    init_params->prefetch_threshold = 0.3;
//    init_params->lookahead = 1;
//    init_params->cache_type = "LRU";
//    init_params->max_meta_data = 0.1;
//    init_params->block_size = 64 * 1024;
//    cache = PG_init(cc_params, (void *) init_params);
//  } else if (strcmp(alg_name, "AMP") == 0) {
//    struct AMP_init_params *AMP_init_params = g_new0(struct AMP_init_params, 1);
//    init_params_g = AMP_init_params;
//    AMP_init_params->APT = 4;
//    AMP_init_params->K = 1;
//    AMP_init_params->p_threshold = 256;
//    AMP_init_params->read_size = 8;
//    cache = AMP_init(cc_params, AMP_init_params);
//  } else if (strcmp(alg_name, "Mithril") == 0) {
//    Mithril_init_params_t *init_params = g_new0(Mithril_init_params_t, 1);
//    init_params_g = init_params;
//    init_params->cache_type = "LRU";
//    init_params->confidence = 0;
//    init_params->lookahead_range = 20;
//    init_params->max_support = 12;
//    init_params->min_support = 3;
//    init_params->pf_list_size = 2;
//    init_params->max_metadata_size = 0.1;
//    init_params->block_size = BLOCK_UNIT_SIZE;
//    init_params->sequential_type = 0;
//    init_params->sequential_K = 0;
////        init_params->recording_loc = miss;
//    init_params->AMP_pthreshold = 256;
//    init_params->cycle_time = 2;
//    init_params->rec_trigger = each_req;
//    cache = Mithril_init(cc_params, init_params);
  } else {
    printf("cannot recognize algorithm %s\n", alg_name);
    exit(1);
  }
  return cache;
}


#endif //MIMIRCACHE_TEST_COMMON_H
