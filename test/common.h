//
// Created by Juncheng Yang on 11/21/19.
//

#ifndef libCacheSim_TEST_COMMON_H
#define libCacheSim_TEST_COMMON_H

#include <glib.h>
#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#ifdef __linux__
#include <sys/sysinfo.h>
#elif __APPLE__
#include <sys/sysctl.h>
#include <sys/types.h>
#endif

#include "../libCacheSim/include/libCacheSim.h"
#include "../libCacheSim/include/libCacheSim/prefetchAlgo.h"

#define BLOCK_UNIT_SIZE 0   // 16 * 1024
#define DISK_SECTOR_SIZE 0  // 512

#define N_TEST_REQ 6
#define N_TEST 6

#define CACHE_SIZE_UNIT (MiB)
#define CACHE_SIZE (1024 * CACHE_SIZE_UNIT)
#define STEP_SIZE (128 * CACHE_SIZE_UNIT)

#define DEFAULT_TTL (300 * 86400)

// static inline unsigned int _n_cores0() {
//   unsigned int eax = 11, ebx = 0, ecx = 1, edx = 0;

//   asm volatile("cpuid"
//                : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx)
//                : "0"(eax), "2"(ecx)
//                :);
//   //  printf("Cores: %d\nThreads: %d\nActual thread: %d\n", eax, ebx, edx);
//   return ebx;
// }

#ifdef __linux__
static inline unsigned int _n_cores(void) { return get_nprocs(); }
#elif __APPLE__
static inline unsigned int _n_cores(void) {
  int count;
  size_t count_len = sizeof(count);
  sysctlbyname("hw.physicalcpu", &count, &count_len, NULL, 0);
  // fprintf(stderr, "you have %i cpu cores", count);
  return count;
}
#else
#error "what platform is this"
#endif

static void _detect_data_path(char *data_path, char *data_name) {
  sprintf(data_path, "data/%s", data_name);
  if (access(data_path, F_OK) != -1) return;

  sprintf(data_path, "../data/%s", data_name);
  if (access(data_path, F_OK) != -1) return;

  sprintf(data_path, "../../data/%s", data_name);
  if (access(data_path, F_OK) != -1) return;

  sprintf(data_path, "../../../data/%s", data_name);
  if (access(data_path, F_OK) != -1) return;

  ERROR("cannot find data %s\n", data_name);
}

static reader_t *setup_oracleGeneralBin_reader(void) {
  char data_path[1024];
  _detect_data_path(data_path, "cloudPhysicsIO.oracleGeneral.bin");
  reader_t *reader_oracle = setup_reader(data_path, ORACLE_GENERAL_TRACE, NULL);
  return reader_oracle;
}

static reader_t *setup_GLCacheTestData_reader(void) {
  char *url =
      "https://ftp.pdl.cmu.edu/pub/datasets/twemcacheWorkload/"
      ".w68.oracleGeneral.bin.zst";
  int ret = system(
      "if [ ! -f .w68.oracleGeneral.bin.zst ]; then wget "
      "https://ftp.pdl.cmu.edu/pub/datasets/twemcacheWorkload/"
      ".w68.oracleGeneral.bin.zst; fi");
  if (ret != 0) {
    ERROR("downloading data failed\n");
  }

  reader_t *reader_oracle = setup_reader(".w68.oracleGeneral.bin.zst", ORACLE_GENERAL_TRACE, NULL);
  return reader_oracle;
}

static reader_t *setup_3LCacheTestData_reader(void) {
  char *url =
      "https://ftp.pdl.cmu.edu/pub/datasets/twemcacheWorkload/cacheDatasets/tencentBlock/"
      "tencentBlock.ns3964.oracleGeneral.zst";
  int ret = system(
      "if [ ! -f tencentBlock.ns3964.oracleGeneral.zst ]; then wget "
      "https://ftp.pdl.cmu.edu/pub/datasets/twemcacheWorkload/cacheDatasets/tencentBlock/"
      "tencentBlock.ns3964.oracleGeneral.zst; fi");
  if (ret != 0) {
    ERROR("downloading data failed\n");
  }

  reader_t *reader_oracle = setup_reader("tencentBlock.ns3964.oracleGeneral.zst", ORACLE_GENERAL_TRACE, NULL);
  return reader_oracle;
}

static reader_t *setup_vscsi_reader_with_ignored_obj_size(void) {
  char data_path[1024];
  reader_init_param_t *init_params = g_new0(reader_init_param_t, 1);
  init_params->ignore_obj_size = true;
  _detect_data_path(data_path, "cloudPhysicsIO.vscsi");
  reader_t *reader_vscsi = setup_reader(data_path, VSCSI_TRACE, init_params);
  g_free(init_params);
  return reader_vscsi;
}

static reader_t *setup_vscsi_reader(void) {
  char data_path[1024];
  _detect_data_path(data_path, "cloudPhysicsIO.vscsi");
  reader_t *reader_vscsi = setup_reader(data_path, VSCSI_TRACE, NULL);
  return reader_vscsi;
}

static reader_t *setup_binary_reader(void) {
  char data_path[1024];
  _detect_data_path(data_path, "cloudPhysicsIO.vscsi");
  reader_init_param_t *init_params_bin = g_new0(reader_init_param_t, 1);
  init_params_bin->binary_fmt_str = "<IIIHHQQ";
  init_params_bin->obj_size_field = 2;
  init_params_bin->obj_id_field = 6;
  init_params_bin->time_field = 7;
  init_params_bin->obj_id_is_num = true;
  reader_t *reader_bin_l = setup_reader(data_path, BIN_TRACE, init_params_bin);
  g_free(init_params_bin);
  return reader_bin_l;
}

static reader_t *setup_csv_reader_obj_str(void) {
  char data_path[1024];
  _detect_data_path(data_path, "cloudPhysicsIO.csv");
  reader_init_param_t *init_params_csv = g_new0(reader_init_param_t, 1);
  init_params_csv->delimiter = ',';
  init_params_csv->time_field = 2;
  init_params_csv->obj_id_field = 5;
  init_params_csv->obj_size_field = 4;
  init_params_csv->has_header = true;
  init_params_csv->obj_id_is_num = false;
  init_params_csv->obj_id_is_num_set = true;
  reader_t *reader_csv_c = setup_reader(data_path, CSV_TRACE, init_params_csv);
  g_free(init_params_csv);
  return reader_csv_c;
}

static reader_t *setup_csv_reader_obj_num(void) {
  char data_path[1024];
  _detect_data_path(data_path, "cloudPhysicsIO.csv");
  reader_init_param_t *init_params_csv = g_new0(reader_init_param_t, 1);
  init_params_csv->delimiter = ',';
  init_params_csv->time_field = 2;
  init_params_csv->obj_id_field = 5;
  init_params_csv->obj_size_field = 4;
  init_params_csv->has_header = true;
  init_params_csv->obj_id_is_num = true;
  reader_t *reader_csv_l = setup_reader(data_path, CSV_TRACE, init_params_csv);
  g_free(init_params_csv);
  return reader_csv_l;
}

static reader_t *setup_plaintxt_reader_num(void) {
  char data_path[1024];
  _detect_data_path(data_path, "cloudPhysicsIO.txt");
  reader_init_param_t init_params = {.obj_id_is_num = true};
  return setup_reader(data_path, PLAIN_TXT_TRACE, &init_params);
}

static reader_t *setup_plaintxt_reader_str(void) {
  char data_path[1024];
  _detect_data_path(data_path, "cloudPhysicsIO.txt");
  reader_init_param_t init_params = {.obj_id_is_num = false};
  return setup_reader(data_path, PLAIN_TXT_TRACE, &init_params);
}

static void test_teardown(gpointer data) {
  reader_t *reader = (reader_t *)data;
  close_reader(reader);
}

static cache_t *create_test_cache(const char *alg_name, common_cache_params_t cc_params, reader_t *reader,
                                  const char *params) {
  cache_t *cache;
  if (strcasecmp(alg_name, "LRU") == 0) {
    cache = LRU_init(cc_params, NULL);
    // } else if (strcasecmp(alg_name, "Clock") == 0) {
    //   cache = Clock_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "FIFO") == 0) {
    cache = FIFO_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "FIFO-Reinsertion") == 0 || strcasecmp(alg_name, "Clock") == 0) {
    cache = Clock_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "ClockPro") == 0) {
    cache = ClockPro_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "Belady") == 0) {
    cache = Belady_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "BeladySize") == 0) {
    cache = BeladySize_init(cc_params, NULL);
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
  } else if (strcasecmp(alg_name, "LFU") == 0) {
    cache = LFU_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "LFUDA") == 0) {
    cache = LFUDA_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "GDSF") == 0) {
    cache = GDSF_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "ARC") == 0) {
    cache = ARC_init(cc_params, NULL);
#if defined(ENABLE_GLCACHE) && ENABLE_GLCACHE == 1
  } else if (strncasecmp(alg_name, "GLCache", 7) == 0) {
    const char *init_params;
    if (strcasecmp(alg_name, "GLCache-OracleLog") == 0) {
      init_params = "type=logOracle, rank-intvl=0.05, retrain-intvl=172800";
    } else if (strcasecmp(alg_name, "GLCache-OracleItem") == 0) {
      init_params = "type=itemOracle, rank-intvl=0.05, retrain-intvl=172800";
    } else if (strcasecmp(alg_name, "GLCache-OracleBoth") == 0) {
      init_params = "type=twoOracle, rank-intvl=0.05, retrain-intvl=172800";
    } else if (strcasecmp(alg_name, "GLCache-LearnedTrueY") == 0) {
      init_params =
          "type=learned, "
          "train-source-y=oracle, rank-intvl=0.05, retrain-intvl=172800";
    } else if (strcasecmp(alg_name, "GLCache-LearnedOnline") == 0) {
      init_params =
          "type=learned, "
          "train-source-y=online, rank-intvl=0.05, retrain-intvl=172800";
    }
    cache = GLCache_init(cc_params, init_params);
#endif
#if defined(ENABLE_3L_CACHE) && ENABLE_3L_CACHE == 1
  } else if (strncasecmp(alg_name, "3LCache", 7) == 0) {
    const char *init_params;
    if (strcasecmp(alg_name, "3LCache-object-miss-ratio") == 0) {
      init_params = "objective=object-miss-ratio";
    } else if (strcasecmp(alg_name, "3LCache-byte-miss-ratio") == 0) {
      init_params = "objective=byte-miss-ratio";
    }
    cache = ThreeLCache_init(cc_params, init_params);
#endif
  } else if (strcasecmp(alg_name, "LHD") == 0) {
    cache = LHD_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "Hyperbolic") == 0) {
    cache = Hyperbolic_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "LeCaR") == 0) {
    cache = LeCaR_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "Cacheus") == 0) {
    cache = Cacheus_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "SR_LRU") == 0) {
    cache = SR_LRU_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "CR_LFU") == 0) {
    cache = CR_LFU_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "SLRU") == 0) {
    cache = SLRU_init(cc_params, "n-seg=5");
  } else if (strcasecmp(alg_name, "LIRS") == 0) {
    cache = LIRS_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "QDLP-FIFO") == 0) {
    cache = QDLP_init(cc_params, "fifo-size-ratio=0.10,main-cache=Clock2");
  } else if (strcasecmp(alg_name, "S3-FIFOv0") == 0) {
    cache = S3FIFOv0_init(cc_params, "move-to-main-threshold=2");
  } else if (strcasecmp(alg_name, "S3-FIFO") == 0) {
    cache = S3FIFO_init(cc_params, "move-to-main-threshold=2");
  } else if (strcasecmp(alg_name, "Sieve") == 0) {
    cache = Sieve_init(cc_params, NULL);
  } else if (strcasecmp(alg_name, "Mithril") == 0) {
    cache = LRU_init(cc_params, NULL);
    cache->prefetcher = create_prefetcher("Mithril", NULL, cc_params.cache_size);
  } else if (strcasecmp(alg_name, "OBL") == 0) {
    cache = LRU_init(cc_params, NULL);
    cache->prefetcher = create_prefetcher("OBL", NULL, cc_params.cache_size);
  } else if (strcasecmp(alg_name, "PG") == 0) {
    cache = LRU_init(cc_params, NULL);
    cache->prefetcher = create_prefetcher("PG", NULL, cc_params.cache_size);
  } else if (strcasecmp(alg_name, "AdaptSize") == 0) {
    cache = LRU_init(cc_params, NULL);
    cache->admissioner = create_adaptsize_admissioner(NULL);
  } else if (strcasecmp(alg_name, "Size") == 0) {
    cache = LRU_init(cc_params, NULL);
    cache->admissioner = create_size_admissioner(NULL);
  } else if (strcasecmp(alg_name, "SizeProb") == 0) {
    cache = LRU_init(cc_params, NULL);
    cache->admissioner = create_size_probabilistic_admissioner(NULL);
  } else if (strcasecmp(alg_name, "BloomFilter") == 0) {
    cache = LRU_init(cc_params, NULL);
    cache->admissioner = create_bloomfilter_admissioner(NULL);
  } else {
    printf("cannot recognize algorithm %s\n", alg_name);
    exit(1);
  }
  return cache;
}

#endif  // libCacheSim_TEST_COMMON_H
