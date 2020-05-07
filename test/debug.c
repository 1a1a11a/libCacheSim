//
// Created by Juncheng Yang on 4/23/20.
//



#include "test_common.h"

int main(int argc, char* argv[]) {
  int n_threads = atoi(argv[2]);
  reader_t *reader = setup_reader(argv[1], TWR_TRACE, OBJ_ID_NUM, NULL);
//  cache_t *cache = create_cache("slabLRC", CACHE_SIZE, reader->base->obj_id_type, NULL);
  cache_t *cache = create_cache("LRU", CACHE_SIZE, reader->base->obj_id_type, NULL);
//  profiler_res_t *res = get_miss_ratio_curve_with_step_size(reader, cache, STEP_SIZE, 0, 4);
  gint num_of_sizes = 4;
//  guint64 cache_sizes[] = {134217728, 145546437, 157831351, 171153180, 185599443, 201265050, 218252919, 236674658, 256651292, 278314063, 301805290, 327279306, 354903467, 384859258, 417343482, 452569551, 490768894, 532192469, 577112421, 625823862, 678646814, 735928313, 798044682, 865404013, 938448839, 1017659047, 1103555030, 1196701102, 1297709212, 1407242959, 1526021952, 1654826540, 1794502940, 1945968791, 2110219187, 2288333213, 2481481036, 2690931591, 2918060917, 3164361198, 3431450569, 3721083741, 4035163535, 4375753380, 4745090868, 5145602457, 5579919413, 6050895092, 6561623691, 7115460573, 7716044314, 8367320603, 9073568169, 9839426887, 10669928264, 11570528495, 12547144304, 13606191823, 14754628737, 16000000000};
  guint64 cache_sizes[] = {15423086663, 32000000000, 64000000000, 128000000000};
  profiler_res_t *res = get_miss_ratio_curve(reader, cache, num_of_sizes, cache_sizes, 0, n_threads);

  for (int i=0; i<num_of_sizes; i++){
    printf("%s cache size %lld req %lld miss %lld req_byte %lld miss_byte %lld\n", __func__,
           (long long) res[i].cache_size, (long long) res[i].req_cnt, (long long) res[i].miss_cnt, (long long) res[i].req_byte, (long long) res[i].miss_byte);
  }
  cache->core->cache_free(cache);
  g_free(res);
}

