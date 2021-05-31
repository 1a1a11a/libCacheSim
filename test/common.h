//
// Created by Juncheng Yang on 11/21/19.
//

#ifndef libCacheSim_TEST_COMMON_H
#define libCacheSim_TEST_COMMON_H


#include "../libCacheSim/include/libCacheSim.h"
#include <unistd.h>
#include <inttypes.h>
#include <glib.h>
#include <glib/gi18n.h>


#define BLOCK_UNIT_SIZE 0    // 16 * 1024
#define DISK_SECTOR_SIZE 0  // 512

#define N_TEST_REQ 6
#define N_TEST 6

#define CACHE_SIZE_UNIT (MiB)
#define CACHE_SIZE (1024 * CACHE_SIZE_UNIT)
#define STEP_SIZE (128 * CACHE_SIZE_UNIT)

#define DEFAULT_TTL (300*86400)

#define NUM_OF_THREADS 4

static void _detect_data_path(char* data_path, char* data_name) {
  sprintf(data_path, "data/%s", data_name);
  if( access(data_path, F_OK ) != -1 )
    return;

  sprintf(data_path, "../data/%s", data_name);
  if( access(data_path, F_OK ) != -1 )
    return;

  sprintf(data_path, "../../data/%s", data_name);
  if( access(data_path, F_OK ) != -1 )
    return;

  sprintf(data_path, "../../../data/%s", data_name);
  if( access(data_path, F_OK ) != -1 )
    return;

  ERROR("cannot find data %s\n", data_name);
}

static reader_t *setup_oracleGeneralBin_reader(void) {
  char data_path[1024];
  _detect_data_path(data_path, "trace.oracleGeneral.bin");
  reader_t *reader_oracle = setup_reader(data_path, ORACLE_GENERAL_BIN, OBJ_ID_NUM, NULL);
  return reader_oracle;
}


static reader_t *setup_vscsi_reader(void) {
  char data_path[1024];
  _detect_data_path(data_path, "trace.vscsi");
  reader_t *reader_vscsi = setup_reader(data_path, VSCSI_TRACE, OBJ_ID_NUM, NULL);
  return reader_vscsi;
}

static reader_t *setup_binary_reader(void) {
  char data_path[1024];
  _detect_data_path(data_path, "trace.vscsi");
  reader_init_param_t *init_params_bin = g_new0(reader_init_param_t, 1);
  strcpy(init_params_bin->binary_fmt, "<3I2H2Q");
  init_params_bin->obj_size_field = 2;
  init_params_bin->obj_id_field = 6;
  init_params_bin->real_time_field = 7;
  reader_t *reader_bin_l = setup_reader(data_path, BIN_TRACE, OBJ_ID_NUM, init_params_bin);
  g_free(init_params_bin);
  return reader_bin_l;
}

static reader_t *setup_csv_reader_obj_str(void) {
  char data_path[1024];
  _detect_data_path(data_path, "trace.csv");
  reader_init_param_t *init_params_csv = g_new0(reader_init_param_t, 1);
  init_params_csv->delimiter = ',';
  init_params_csv->real_time_field = 2;
  init_params_csv->obj_id_field = 5;
  init_params_csv->obj_size_field = 4;
  init_params_csv->has_header = TRUE;
  reader_t *reader_csv_c = setup_reader(data_path, CSV_TRACE, OBJ_ID_STR, init_params_csv);
  g_free(init_params_csv);
  return reader_csv_c;
}

static reader_t *setup_csv_reader_obj_num(void) {
  char data_path[1024];
  _detect_data_path(data_path, "trace.csv");
  reader_init_param_t *init_params_csv = g_new0(reader_init_param_t, 1);
  init_params_csv->delimiter = ',';
  init_params_csv->real_time_field = 2;
  init_params_csv->obj_id_field = 5;
  init_params_csv->obj_size_field = 4;
  init_params_csv->has_header = TRUE;
  reader_t *reader_csv_l = setup_reader(data_path, CSV_TRACE, OBJ_ID_NUM, init_params_csv);
  g_free(init_params_csv);
  return reader_csv_l;
}

static reader_t *setup_plaintxt_reader_num(void) {
  char data_path[1024];
  _detect_data_path(data_path, "trace.txt");
  return setup_reader(data_path, PLAIN_TXT_TRACE, OBJ_ID_NUM, NULL);
}

static reader_t *setup_plaintxt_reader_str(void) {
  char data_path[1024];
  _detect_data_path(data_path, "trace.txt");
  return setup_reader(data_path, PLAIN_TXT_TRACE, OBJ_ID_STR, NULL);
}


static void test_teardown(gpointer data) {
  reader_t *reader = (reader_t *) data;
  close_reader(reader);
}

static cache_t *create_test_cache(const char *alg_name,
                           common_cache_params_t cc_params, reader_t* reader, void *params) {
  cache_t *cache;
  if (strcasecmp(alg_name, "LRU") == 0) {
    cache = LRU_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "Clock") == 0) {
    cache = Clock_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "FIFO") == 0) {
    cache = FIFO_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "Optimal") == 0) {
    cache = Optimal_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "LRUv0") == 0) {
    cache = LRUv0_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "Random") == 0) {
    cache = Random_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "MRU") == 0) {
    cache = MRU_init(cc_params, NULL);
//  } else if (strcmp(alg_name, "LRU_K") == 0) {
//    cache = LRU_K_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "LFU") == 0) {
    cache = LFU_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "LFUFast") == 0) {
    cache = LFUFast_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "LFUDA") == 0) {
    cache = LFUDA_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "ARC") == 0) {
    ARC_init_params_t *init_params = my_malloc_n(ARC_init_params_t, 1);
    init_params->ghost_list_factor = 1;
    cache = ARC_init(cc_params, init_params);
  } else if (strcasecmp(alg_name, "LHD") == 0) {
    cache = LHD_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "Hyperbolic") == 0) {
    cache = Hyperbolic_init(cc_params, NULL);

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
  } else if (strcasecmp(alg_name, "slabLRC") == 0) {
    cache = slabLRC_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "slabLRU") == 0) {
    cache = slabLRU_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "slabObjLRU") == 0) {
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


#endif //libCacheSim_TEST_COMMON_H
