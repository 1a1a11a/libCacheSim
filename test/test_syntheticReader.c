#include <fenv.h>
#include <math.h>

#include "common.h"
#include "traceReader/generalReader/syntheticRng.h"

static reader_t *open_synthetic(const char *spec) {
  return setup_reader(spec, SYNTHETIC_TRACE, NULL);
}

static void read_id(reader_t *reader, request_t *req, uint64_t expected) {
  g_assert_cmpint(read_one_req(reader, req), ==, 0);
  g_assert_true(req->valid);
  g_assert_cmpuint(req->obj_id, ==, expected);
  g_assert_cmpint(req->obj_size, ==, 1);
  g_assert_cmpint(req->obj_cost, ==, 1);
  g_assert_cmpint(req->clock_time, ==, 0);
  g_assert_cmpint(req->next_access_vtime, ==, -2);
}

static void test_random_vectors(void) {
  /* Published Philox2x64-10 known-answer vectors from
   * Random123/tests/kat_vectors. Check the first output word, including
   * full-width counter and key values. */
  g_assert_cmpuint(synthetic_random(0, 0, 0), ==, UINT64_C(0xca00a0459843d731));
  g_assert_cmpuint(synthetic_random(UINT64_MAX, UINT64_MAX, UINT64_MAX), ==,
                   UINT64_C(0x65b021d60cd8310f));
  g_assert_cmpuint(synthetic_random(UINT64_C(0x243f6a8885a308d3),
                                    UINT64_C(0x13198a2e03707344),
                                    UINT64_C(0xa4093822299f31d0)),
                   ==, UINT64_C(0x0a5e742c2997341c));
}

static void test_navigation(gconstpointer data) {
  reader_t *reader = open_synthetic(data);
  request_t *req = new_request();
  uint64_t ids[1000];
  g_assert_cmpint(get_num_of_req(reader), ==, 1000);
  g_assert_null(reader->mapped_file);
  g_assert_null(reader->file);
  g_assert_cmpuint(reader->file_size, ==, 0);
  for (size_t i = 0; i < G_N_ELEMENTS(ids); i++) {
    g_assert_cmpint(read_one_req(reader, req), ==, 0);
    g_assert_cmpuint(req->obj_id, <, 100);
    ids[i] = req->obj_id;
  }
  for (int i = 0; i < 3; i++) {
    g_assert_cmpint(read_one_req(reader, req), ==, 1);
    g_assert_false(req->valid);
  }
  reset_reader(reader);
  for (size_t i = 0; i < G_N_ELEMENTS(ids); i++) {
    /* Consumers may reuse request objects; source fields must be restored. */
    req->clock_time = 100;
    req->obj_size = 999;
    req->obj_cost = 99;
    req->next_access_vtime = 123;
    next_rand();
    read_id(reader, req, ids[i]);
  }
  reset_reader(reader);
  g_assert_cmpint(skip_n_req(reader, 123), ==, 123);
  reader_t *clone = clone_reader(reader);
  read_id(clone, req, ids[0]);
  read_id(reader, req, ids[123]);
  read_first_req(reader, req);
  g_assert_cmpuint(req->obj_id, ==, ids[0]);
  read_last_req(reader, req);
  g_assert_cmpuint(req->obj_id, ==, ids[999]);
  g_assert_cmpint(reader->n_read_req, ==, 1);
  read_id(reader, req, ids[124]);
  reader_set_read_pos(reader, 0.5);
  read_id(reader, req, ids[500]);
  reader_set_read_pos(reader, 1);
  g_assert_cmpint(go_back_one_req(reader), ==, 0);
  read_id(reader, req, ids[999]);
  for (int i = 998; i >= 0; i--) {
    g_assert_cmpint(read_one_req_above(reader, req), ==, 0);
    g_assert_cmpuint(req->obj_id, ==, ids[i]);
  }
  g_assert_cmpint(read_one_req_above(reader, req), ==, 1);
  reset_reader(reader);
  g_assert_cmpint(skip_n_req(reader, 2000), ==, 1000);
  g_assert_cmpint(skip_n_req(reader, 1), ==, 0);
  g_assert_cmpint(skip_n_req(reader, -1), ==, 0);
  reader_set_read_pos(reader, -1);
  read_id(reader, req, ids[0]);
  reader_set_read_pos(reader, NAN);
  read_id(reader, req, ids[0]);
  reader_set_read_pos(reader, INFINITY);
  g_assert_cmpint(read_one_req(reader, req), ==, 1);
  close_reader(reader);
  /* A clone must own its state, including after the original is closed. */
  read_id(clone, req, ids[1]);
  close_reader(clone);
  free_request(req);
}

static void test_parameters(void) {
  request_t *req = new_request();
  reader_t *short_trace = open_synthetic("Uniform_100_17");
  reader_t *long_trace = open_synthetic("Zipf_200_17_0_1");
  reader_t *other_seed = open_synthetic("Uniform_100_17_2");
  request_t *other = new_request();
  bool different = false;
  for (int i = 0; i < 100; i++) {
    g_assert_cmpint(read_one_req(short_trace, req), ==, 0);
    read_id(long_trace, other, req->obj_id);
    read_one_req(other_seed, other);
    different |= other->obj_id != req->obj_id;
  }
  g_assert_true(different);
  close_reader(short_trace);
  close_reader(long_trace);
  close_reader(other_seed);

  reader_t *large = open_synthetic(
      "Uniform_9223372036854775807_2147483647_18446744073709551615");
  g_assert_cmpint(get_num_of_req(large), ==, INT64_MAX);
  reader_set_read_pos(large, 1);
  g_assert_cmpint(go_back_one_req(large), ==, 0);
  g_assert_cmpint(read_one_req(large, req), ==, 0);
  g_assert_cmpuint(req->obj_id, <, INT32_MAX);
  g_assert_cmpint(read_one_req(large, req), ==, 1);
  close_reader(large);

  const char *edges[] = {"Zipf_100_1_1", "Zipf_100_2_100",
                         "Zipf_100_2147483647_1e300",
                         "Zipf_100_2147483647_1.7976931348623157e308"};
  for (size_t i = 0; i < G_N_ELEMENTS(edges); i++) {
    feclearexcept(FE_ALL_EXCEPT);
    reader_t *reader = open_synthetic(edges[i]);
    for (int j = 0; j < 100; j++) read_id(reader, req, 0);
    g_assert_cmpint(fetestexcept(FE_INVALID | FE_DIVBYZERO | FE_OVERFLOW), ==,
                    0);
    close_reader(reader);
  }
  free_request(other);
  free_request(req);
}

static void test_limits_and_sampling(void) {
  const int caps[] = {-1, 0, 1, 2, 99, 100, 101};
  request_t *req = new_request();
  for (size_t i = 0; i < G_N_ELEMENTS(caps); i++) {
    reader_init_param_t init = default_reader_init_params();
    init.cap_at_n_req = caps[i];
    reader_t *reader = setup_reader("Uniform_100_10", SYNTHETIC_TRACE, &init);
    int expected = caps[i] > 0 ? MIN(caps[i], 100) : 100;
    for (int pass = 0; pass < 2; pass++) {
      int n = 0;
      while (read_one_req(reader, req) == 0) n++;
      g_assert_cmpint(n, ==, expected);
      reset_reader(reader);
    }
    close_reader(reader);
  }

  for (int temporal = 0; temporal < 2; temporal++) {
    sampler_t *sampler =
        temporal ? create_temporal_sampler(0.25) : create_spatial_sampler(0.25);
    reader_init_param_t init = default_reader_init_params();
    init.sampler = sampler;
    reader_t *sampled =
        setup_reader("Zipf_1000_100_0.9", SYNTHETIC_TRACE, &init);
    reader_t *raw = open_synthetic("Zipf_1000_100_0.9");
    uint64_t ids[1000];
    int count = 0;
    while (read_one_req(raw, req) == 0) {
      if (sampler->sample(sampler, req)) ids[count++] = req->obj_id;
    }
    sampler->free(sampler);
    close_reader(raw);
    g_assert_cmpint(count, >, 0);
    for (int pass = 0; pass < 2; pass++) {
      for (int i = 0; i < count; i++) read_id(sampled, req, ids[i]);
      g_assert_cmpint(read_one_req(sampled, req), ==, 1);
      reset_reader(sampled);
    }
    if (!temporal) {
      read_id(sampled, req, ids[0]);
      int64_t n_read = sampled->n_read_req;
      read_first_req(sampled, req);
      g_assert_cmpuint(req->obj_id, ==, ids[0]);
      read_last_req(sampled, req);
      g_assert_cmpuint(req->obj_id, ==, ids[count - 1]);
      g_assert_cmpint(sampled->n_read_req, ==, n_read);
      read_id(sampled, req, ids[1]);
      reader_set_read_pos(sampled, 1);
      sampled->read_direction = READ_BACKWARD;
      g_assert_cmpint(go_back_one_req(sampled), ==, 0);
      read_id(sampled, req, ids[count - 1]);
      for (int i = count - 2; i >= 0; i--) {
        g_assert_cmpint(read_one_req_above(sampled, req), ==, 0);
        g_assert_cmpuint(req->obj_id, ==, ids[i]);
      }
      g_assert_cmpint(read_one_req_above(sampled, req), ==, 1);
      sampled->read_direction = READ_FORWARD;
      reset_reader(sampled);
    }
    reader_t *clone = clone_reader(sampled);
    close_reader(sampled);
    for (int i = 0; i < count; i++) read_id(clone, req, ids[i]);
    close_reader(clone);
  }
  free_request(req);
}

static void test_sampled_boundaries(void) {
  request_t *req = new_request();
  sampler_t *sampler = create_spatial_sampler(0.25);
  req->obj_id = 0;
  req->hv = 0;
  while (sampler->sample(sampler, req)) sampler->sampling_salt++;
  reader_init_param_t init = default_reader_init_params();
  init.sampler = sampler;
  reader_t *reader = setup_reader("Uniform_100_1", SYNTHETIC_TRACE, &init);
  g_assert_cmpint(read_one_req(reader, req), ==, 1);
  g_assert_false(req->valid);
  read_first_req(reader, req);
  g_assert_false(req->valid);
  read_last_req(reader, req);
  g_assert_false(req->valid);
  close_reader(reader);

  const int caps[] = {1, 2, 17, 100};
  for (size_t i = 0; i < G_N_ELEMENTS(caps); i++) {
    init.cap_at_n_req = caps[i];
    reader = setup_reader("Uniform_100_10", SYNTHETIC_TRACE, &init);
    reader_t *raw = open_synthetic("Uniform_100_10");
    uint64_t ids[100];
    int count = 0;
    for (int j = 0; j < caps[i]; j++) {
      g_assert_cmpint(read_one_req(raw, req), ==, 0);
      if (sampler->sample(sampler, req)) ids[count++] = req->obj_id;
    }
    for (int pass = 0; pass < 2; pass++) {
      for (int j = 0; j < count; j++) read_id(reader, req, ids[j]);
      g_assert_cmpint(read_one_req(reader, req), ==, 1);
      g_assert_cmpint(reader->n_read_req, ==, caps[i]);
      reset_reader(reader);
    }
    close_reader(raw);
    close_reader(reader);
  }
  sampler->free(sampler);
  free_request(req);
}

static void test_distribution(void) {
  /* Independent finite-PMF calculation, not the sampler's integral formula.
   * Fixed seeds and generous 7-sigma bounds avoid flaky tail-bin tests. */
  const double alphas[] = {0, 1e-10, 0.8, 1 - 1e-9, 1, 1 + 1e-9, 1.2, 2, 8};
  const int n = 200000, m = 16;
  request_t *req = new_request();
  for (size_t a = 0; a < G_N_ELEMENTS(alphas); a++) {
    char spec[128];
    snprintf(spec, sizeof(spec), "Zipf_%d_%d_%.17g_42", n, m, alphas[a]);
    reader_t *reader = open_synthetic(spec);
    int counts[16] = {0};
    int same = 0;
    uint64_t previous = UINT64_MAX;
    for (int i = 0; i < n; i++) {
      g_assert_cmpint(read_one_req(reader, req), ==, 0);
      g_assert_cmpuint(req->obj_id, <, m);
      counts[req->obj_id]++;
      same += req->obj_id == previous;
      previous = req->obj_id;
    }
    long double weights[16], sum = 0;
    for (int k = 0; k < m; k++) {
      weights[k] = powl(k + 1, -alphas[a]);
      sum += weights[k];
    }
    long double collision = 0;
    int tail_count = 0;
    long double tail_prob = 0;
    for (int k = 0; k < m; k++) {
      long double p = weights[k] / sum;
      collision += p * p;
      if (n * p < 100) {
        tail_count += counts[k];
        tail_prob += p;
      } else {
        g_assert_true(fabsl(counts[k] - n * p) <
                      7 * sqrtl(n * p * (1 - p)) + 2);
      }
    }
    g_assert_true(fabsl(tail_count - n * tail_prob) <
                  7 * sqrtl(n * tail_prob * (1 - tail_prob)) + 2);
    /* Adjacent collision events overlap: use a conservative O(sqrt(n))
     * tolerance rather than assuming independent Bernoulli observations. */
    g_assert_true(fabsl(same - (n - 1) * collision) < 10 * sqrtl(n));
    close_reader(reader);
  }
  free_request(req);
}

static void compare_stats(const cache_stat_t *a, const cache_stat_t *b) {
  g_assert_cmpuint(a->n_req, ==, b->n_req);
  g_assert_cmpuint(a->n_miss, ==, b->n_miss);
  g_assert_cmpuint(a->n_req_byte, ==, b->n_req_byte);
  g_assert_cmpuint(a->n_miss_byte, ==, b->n_miss_byte);
}

static void test_materialized(gconstpointer data) {
  reader_t *synthetic = open_synthetic(data);
  request_t *req = new_request();
  char *path = NULL;
  int fd = g_file_open_tmp("libcachesim-synthetic-XXXXXX", &path, NULL);
  g_assert_cmpint(fd, >=, 0);
  FILE *file = fdopen(fd, "w");
  g_assert_nonnull(file);
  while (read_one_req(synthetic, req) == 0) {
    fprintf(file, "%" PRIu64 "\n", req->obj_id);
  }
  g_assert_cmpint(fclose(file), ==, 0);
  reader_init_param_t init = default_reader_init_params();
  reader_t *materialized = setup_reader(path, PLAIN_TXT_TRACE, &init);
  request_t *other = new_request();
  reset_reader(synthetic);
  while (read_one_req(synthetic, req) == 0) {
    read_id(materialized, other, req->obj_id);
  }
  g_assert_cmpint(read_one_req(materialized, other), ==, 1);

  const char *algorithms[] = {"lru", "fifo", "s3fifo"};
  uint64_t sizes[] = {10, 20, 50, 100};
  common_cache_params_t cc = {
      .cache_size = 10, .hashpower = 8, .consider_obj_metadata = false};
  for (size_t a = 0; a < G_N_ELEMENTS(algorithms); a++) {
    cache_t *cache = create_cache_by_name(algorithms[a], cc, NULL);
    for (int warmup = 0; warmup < 2; warmup++) {
      double fraction = warmup ? 0.1 : 0;
      cache_stat_t *serial = simulate_at_multi_sizes(
          synthetic, cache, 4, sizes, NULL, fraction, 0, 1, false);
      cache_stat_t *parallel = simulate_at_multi_sizes(
          synthetic, cache, 4, sizes, NULL, fraction, 0, 4, false);
      cache_stat_t *disk = simulate_at_multi_sizes(
          materialized, cache, 4, sizes, NULL, fraction, 0, 2, false);
      for (int i = 0; i < 4; i++) {
        compare_stats(&serial[i], &parallel[i]);
        compare_stats(&serial[i], &disk[i]);
      }
      free(serial);
      free(parallel);
      free(disk);
    }
    cache->cache_free(cache);
  }
  free_request(other);
  free_request(req);
  close_reader(synthetic);
  close_reader(materialized);
  g_assert_cmpint(unlink(path), ==, 0);
  g_free(path);
}

int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);
  g_test_add_func("/synthetic/random-vectors", test_random_vectors);
  g_test_add_data_func("/synthetic/uniform-navigation", "Uniform_1000_100_7",
                       test_navigation);
  g_test_add_data_func("/synthetic/zipf-navigation", "Zipf_1000_100_0.9_7",
                       test_navigation);
  g_test_add_func("/synthetic/parameters", test_parameters);
  g_test_add_func("/synthetic/sampling-and-limits", test_limits_and_sampling);
  g_test_add_func("/synthetic/sampled-boundaries", test_sampled_boundaries);
  g_test_add_func("/synthetic/distributions", test_distribution);
  g_test_add_data_func("/synthetic/uniform-materialized", "Uniform_5000_100_7",
                       test_materialized);
  g_test_add_data_func("/synthetic/zipf-materialized", "Zipf_5000_100_0.9_7",
                       test_materialized);
  return g_test_run();
}
