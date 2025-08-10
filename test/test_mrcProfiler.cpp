//
// Created by Xiaojun Guo on 02/28/25.
//

#include "common.h"
#include "mrcProfiler/mrcProfiler.h"

/**
 * this one for testing with the SHARDS profiler with fiexd sample rate
 * @param user_data
 */
static void test_shards_profiler_with_fixed_sample_rate(
    gconstpointer user_data) {
  reader_t *reader = setup_vscsi_reader();
  mrcProfiler::mrc_profiler_params_t params;
  mrcProfiler::mrc_profiler_e mrc_profiler_type = mrcProfiler::SHARDS_PROFILER;

  params.cache_algorithm_str = "LRU";
  params.shards_params.parse_params("FIX_RATE,0.01,10");
  uint64_t step_size = 202976972;
  int test_steps = 10;
  for (int i = 0; i < test_steps; i++) {
    params.profile_size.push_back(step_size * (i + 1));
  }

  mrcProfiler::MRCProfilerBase *profiler =
      create_mrc_profiler(mrc_profiler_type, reader, "", params);
  g_assert_true(profiler != NULL);
  profiler->run();

  std::vector<size_t> mrc_size_vec = profiler->get_mrc_size_vec();
  std::vector<int64_t> hit_cnt_vec = profiler->get_hit_cnt_vec();
  std::vector<int64_t> hit_size_vec = profiler->get_hit_size_vec();

  g_assert_cmpuint(profiler->get_n_req(), ==, 113872);
  g_assert_cmpuint(profiler->get_sum_obj_size_req(), ==, 4205978112);
  g_assert_cmpuint(mrc_size_vec.size(), ==, test_steps);
  g_assert_cmpuint(hit_cnt_vec.size(), ==, test_steps);
  g_assert_cmpuint(hit_size_vec.size(), ==, test_steps);

  for (int i = 0; i < test_steps; i++) {
    g_assert_cmpuint(mrc_size_vec[i], ==, step_size * (i + 1));
  }

  g_assert_cmpuint(hit_cnt_vec[0], ==, 27972);
  g_assert_cmpuint(hit_cnt_vec[1], ==, 34472);
  g_assert_cmpuint(hit_cnt_vec[2], ==, 42272);
  g_assert_cmpuint(hit_cnt_vec[3], ==, 43372);
  g_assert_cmpuint(hit_cnt_vec[4], ==, 43672);
  g_assert_cmpuint(hit_cnt_vec[5], ==, 45472);
  g_assert_cmpuint(hit_cnt_vec[6], ==, 49172);
  g_assert_cmpuint(hit_cnt_vec[7], ==, 64272);
  g_assert_cmpuint(hit_cnt_vec[8], ==, 64272);
  g_assert_cmpuint(hit_cnt_vec[9], ==, 64272);

  g_assert_cmpuint(hit_size_vec[0], ==, 489574912);
  g_assert_cmpuint(hit_size_vec[1], ==, 702566912);
  g_assert_cmpuint(hit_size_vec[2], ==, 1152461312);
  g_assert_cmpuint(hit_size_vec[3], ==, 1212416512);
  g_assert_cmpuint(hit_size_vec[4], ==, 1226957312);
  g_assert_cmpuint(hit_size_vec[5], ==, 1344922112);
  g_assert_cmpuint(hit_size_vec[6], ==, 1510246912);
  g_assert_cmpuint(hit_size_vec[7], ==, 2151475712);
  g_assert_cmpuint(hit_size_vec[8], ==, 2151475712);
  g_assert_cmpuint(hit_size_vec[9], ==, 2151475712);

  delete profiler;

  close_reader(reader);
}

/**
 * this one for testing with the SHARDS profiler with fiexd sample size
 * @param user_data
 */
static void test_shards_profiler_with_fixed_sample_size(
    gconstpointer user_data) {
  reader_t *reader = setup_vscsi_reader();
  mrcProfiler::mrc_profiler_params_t params;
  mrcProfiler::mrc_profiler_e mrc_profiler_type = mrcProfiler::SHARDS_PROFILER;

  params.cache_algorithm_str = "LRU";
  params.shards_params.parse_params("FIX_SIZE,8192,10");
  uint64_t step_size = 202976972;
  int test_steps = 10;
  for (int i = 0; i < test_steps; i++) {
    params.profile_size.push_back(step_size * (i + 1));
  }

  mrcProfiler::MRCProfilerBase *profiler =
      create_mrc_profiler(mrc_profiler_type, reader, "", params);
  g_assert_true(profiler != NULL);
  profiler->run();

  std::vector<size_t> mrc_size_vec = profiler->get_mrc_size_vec();
  std::vector<int64_t> hit_cnt_vec = profiler->get_hit_cnt_vec();
  std::vector<int64_t> hit_size_vec = profiler->get_hit_size_vec();

  g_assert_cmpuint(profiler->get_n_req(), ==, 113872);
  g_assert_cmpuint(profiler->get_sum_obj_size_req(), ==, 4205978112);
  g_assert_cmpuint(mrc_size_vec.size(), ==, test_steps);
  g_assert_cmpuint(hit_cnt_vec.size(), ==, test_steps);
  g_assert_cmpuint(hit_size_vec.size(), ==, test_steps);

  for (int i = 0; i < test_steps; i++) {
    g_assert_cmpuint(mrc_size_vec[i], ==, step_size * (i + 1));
  }

  g_assert_cmpuint(hit_cnt_vec[0], ==, 22739);
  g_assert_cmpuint(hit_cnt_vec[1], ==, 31005);
  g_assert_cmpuint(hit_cnt_vec[2], ==, 33051);
  g_assert_cmpuint(hit_cnt_vec[3], ==, 41681);
  g_assert_cmpuint(hit_cnt_vec[4], ==, 41883);
  g_assert_cmpuint(hit_cnt_vec[5], ==, 44488);
  g_assert_cmpuint(hit_cnt_vec[6], ==, 48662);
  g_assert_cmpuint(hit_cnt_vec[7], ==, 62647);
  g_assert_cmpuint(hit_cnt_vec[8], ==, 64735);
  g_assert_cmpuint(hit_cnt_vec[9], ==, 64758);

  g_assert_cmpuint(hit_size_vec[0], ==, 274746659);
  g_assert_cmpuint(hit_size_vec[1], ==, 562343096);
  g_assert_cmpuint(hit_size_vec[2], ==, 627357922);
  g_assert_cmpuint(hit_size_vec[3], ==, 1143647463);
  g_assert_cmpuint(hit_size_vec[4], ==, 1152787147);
  g_assert_cmpuint(hit_size_vec[5], ==, 1318274897);
  g_assert_cmpuint(hit_size_vec[6], ==, 1491688049);
  g_assert_cmpuint(hit_size_vec[7], ==, 2033475145);
  g_assert_cmpuint(hit_size_vec[8], ==, 2178659536);
  g_assert_cmpuint(hit_size_vec[9], ==, 2178825309);

  delete profiler;

  close_reader(reader);
}

/**
 * this one for testing with the minisim profiler with fiexd sample rate
 * @param user_data
 */
static void test_minisim_profiler_with_fixed_sample_rate(
    gconstpointer user_data) {
  reader_t *reader = setup_vscsi_reader();
  mrcProfiler::mrc_profiler_params_t params;
  mrcProfiler::mrc_profiler_e mrc_profiler_type = mrcProfiler::MINISIM_PROFILER;

  params.cache_algorithm_str = "FIFO";
  params.minisim_params.parse_params("FIX_RATE,0.01,1");
  uint64_t step_size = 202976972;
  int test_steps = 10;
  for (int i = 0; i < test_steps; i++) {
    params.profile_size.push_back(step_size * (i + 1));
  }

  mrcProfiler::MRCProfilerBase *profiler =
      create_mrc_profiler(mrc_profiler_type, reader, "", params);
  g_assert_true(profiler != NULL);
  profiler->run();

  std::vector<size_t> mrc_size_vec = profiler->get_mrc_size_vec();
  std::vector<int64_t> hit_cnt_vec = profiler->get_hit_cnt_vec();
  std::vector<int64_t> hit_size_vec = profiler->get_hit_size_vec();

  g_assert_cmpuint(profiler->get_n_req(), ==, 113872);
  g_assert_cmpuint(profiler->get_sum_obj_size_req(), ==, 4205978112);
  g_assert_cmpuint(mrc_size_vec.size(), ==, test_steps);
  g_assert_cmpuint(hit_cnt_vec.size(), ==, test_steps);
  g_assert_cmpuint(hit_size_vec.size(), ==, test_steps);

  for (int i = 0; i < test_steps; i++) {
    g_assert_cmpuint(mrc_size_vec[i], ==, step_size * (i + 1));
  }

  g_assert_cmpuint(hit_cnt_vec[0], ==, 21172);
  g_assert_cmpuint(hit_cnt_vec[1], ==, 29472);
  g_assert_cmpuint(hit_cnt_vec[2], ==, 39172);
  g_assert_cmpuint(hit_cnt_vec[3], ==, 39572);
  g_assert_cmpuint(hit_cnt_vec[4], ==, 39672);
  g_assert_cmpuint(hit_cnt_vec[5], ==, 39672);
  g_assert_cmpuint(hit_cnt_vec[6], ==, 39772);
  g_assert_cmpuint(hit_cnt_vec[7], ==, 42072);
  g_assert_cmpuint(hit_cnt_vec[8], ==, 62272);
  g_assert_cmpuint(hit_cnt_vec[9], ==, 62372);

  g_assert_cmpuint(hit_size_vec[0], ==, 128461312);
  g_assert_cmpuint(hit_size_vec[1], ==, 463616512);
  g_assert_cmpuint(hit_size_vec[2], ==, 983962112);
  g_assert_cmpuint(hit_size_vec[3], ==, 1001165312);
  g_assert_cmpuint(hit_size_vec[4], ==, 1007718912);
  g_assert_cmpuint(hit_size_vec[5], ==, 1007718912);
  g_assert_cmpuint(hit_size_vec[6], ==, 1009766912);
  g_assert_cmpuint(hit_size_vec[7], ==, 1126195712);
  g_assert_cmpuint(hit_size_vec[8], ==, 2044774912);
  g_assert_cmpuint(hit_size_vec[9], ==, 2046822912);

  delete profiler;

  close_reader(reader);
}

int main(int argc, char *argv[]) {
  g_test_init(&argc, &argv, NULL);
  g_test_set_nonfatal_assertions();

  g_test_add_data_func(
      "/libCacheSim/test_shards_profiler_with_fixed_sample_rate", NULL,
      test_shards_profiler_with_fixed_sample_rate);

  g_test_add_data_func(
      "/libCacheSim/test_shards_profiler_with_fixed_sample_size", NULL,
      test_shards_profiler_with_fixed_sample_size);

  g_test_add_data_func(
      "/libCacheSim/test_minisim_profiler_with_fixed_sample_rate", NULL,
      test_minisim_profiler_with_fixed_sample_rate);

  return g_test_run();
}
