/*
 * test_MRC.c
 *
 * Integration tests for the MRC tool: run the MRC binary in SHARDS and in MINI
 * mode on data/cloudPhysicsIO.vscsi and check the CSV it writes.
 *
 * The build passes in the path of the MRC binary (MRC_BINARY) and of the
 * sample traces (MRC_TEST_DATA_DIR), and each test has MRC write into a new
 * temporary directory, so the test does not depend on the name of the build
 * directory or on the directory it runs from.
 *
 * Every run must also exit 0. MRC is built with the same flags as this test,
 * so in the LeakSanitizer CI build a leak in MRC turns its exit status into
 * 23, which fails the test.
 */

#include <glib.h>
#include <glib/gstdio.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>

#ifndef MRC_BINARY
#error "MRC_BINARY must be the path of the MRC binary, see test/CMakeLists.txt"
#endif
#ifndef MRC_TEST_DATA_DIR
#error "MRC_TEST_DATA_DIR must be the sample trace dir, see test/CMakeLists.txt"
#endif

#define DISTANCE_TOLERANCE 3
#define FLOAT_TOLERANCE 0.00001

typedef struct {
  gchar *trace_path; /* the sample trace the tests run on */
  gchar *out_dir;    /* a new temporary directory for this test */
  gchar *csv_path;   /* the CSV MRC is told to write, inside out_dir */
} MRCFixture;

static void mrc_fixture_set_up(MRCFixture *fixture, gconstpointer user_data) {
  GError *error = NULL;
  fixture->trace_path =
      g_build_filename(MRC_TEST_DATA_DIR, "cloudPhysicsIO.vscsi", NULL);
  fixture->out_dir = g_dir_make_tmp("libCacheSim-testMRC-XXXXXX", &error);
  g_assert_no_error(error);
  fixture->csv_path = g_build_filename(fixture->out_dir, "mrc.csv", NULL);
}

static void mrc_fixture_tear_down(MRCFixture *fixture,
                                  gconstpointer user_data) {
  g_assert_cmpint(g_remove(fixture->csv_path), ==, 0);
  g_assert_cmpint(g_rmdir(fixture->out_dir), ==, 0);
  g_free(fixture->csv_path);
  g_free(fixture->out_dir);
  g_free(fixture->trace_path);
}

/* Runs MRC with the given NULL-terminated argv and asserts that it exits 0.
 * MRC writes to this test's stdout and stderr, so ctest --output-on-failure
 * shows its log, including any LeakSanitizer report. */
static void run_mrc(const gchar **argv) {
  gint wait_status = 0;
  GError *error = NULL;

  fflush(stdout);
  /* g_spawn_sync takes a gchar ** but does not modify argv */
  g_spawn_sync(NULL, (gchar **)argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL,
               NULL, &wait_status, &error);
  g_assert_no_error(error);
  g_assert_true(WIFEXITED(wait_status));
  g_assert_cmpint(WEXITSTATUS(wait_status), ==, 0);
}

/* Returns the lines of the CSV at csv_path; free them with g_strfreev. */
static gchar **read_csv_lines(const gchar *csv_path) {
  gchar *contents = NULL;
  GError *error = NULL;

  g_file_get_contents(csv_path, &contents, NULL, &error);
  g_assert_no_error(error);
  gchar **lines = g_strsplit(contents, "\n", -1);
  g_free(contents);
  return lines;
}

typedef struct {
  const char *expected_distance_str;  // if not NULL, compare string exactly
  int expected_distance_num;  // if expected_distance_str == NULL, use numeric
                              // comparison
  int expected_frequency;
} ExpectedRow;

/* Rows of the SHARDS histogram of cloudPhysicsIO.vscsi at rate 0.1, e.g.
 *   Distance,Frequency
 *   ColdMiss,4826
 *   14829,1
 *   ...
 * MRC writes the distance rows in GLib hash table order, so each expected row
 * is looked up by its distance rather than by its line number. Numeric
 * distances match within DISTANCE_TOLERANCE: MRC scales each sampled distance
 * by 1/rate. */
static const ExpectedRow expected_shards_rows[] = {
    {"ColdMiss", 0, 4826}, {NULL, 14829, 1}, {NULL, 9279, 2},
    {NULL, 0, 749},        {NULL, 9, 555},   {NULL, 31529, 4},
};

static gboolean distance_matches(const gchar *distance,
                                 const ExpectedRow *expected) {
  if (expected->expected_distance_str != NULL) {
    return strcmp(distance, expected->expected_distance_str) == 0;
  }
  /* skip ColdMiss and Overflow, which atoi would read as distance 0 */
  if (!g_ascii_isdigit(distance[0])) {
    return FALSE;
  }
  return abs(atoi(distance) - expected->expected_distance_num) <=
         DISTANCE_TOLERANCE;
}

/* Integration test for SHARDS.
 * Runs MRC SHARDS <csv> cloudPhysicsIO.vscsi vscsi 0.1 and checks that each
 * expected row appears exactly once in the histogram it writes. */
static void test_shards_csv_integration(MRCFixture *fixture,
                                        gconstpointer user_data) {
  const gchar *argv[] = {
      MRC_BINARY, "SHARDS", fixture->csv_path, fixture->trace_path, "vscsi",
      "0.1",      NULL};
  run_mrc(argv);

  gchar **lines = read_csv_lines(fixture->csv_path);
  g_assert_cmpstr(g_strstrip(lines[0]), ==, "Distance,Frequency");

  for (gsize i = 0; i < G_N_ELEMENTS(expected_shards_rows); i++) {
    const ExpectedRow *expected = &expected_shards_rows[i];
    int n_matches = 0;
    for (int j = 1; lines[j] != NULL; j++) {
      gchar *line = g_strstrip(lines[j]);
      if (line[0] == '\0') continue;  // after the final newline

      gchar **tokens = g_strsplit(line, ",", 0);
      // Expect two columns: Distance and Frequency
      g_assert_cmpint(g_strv_length(tokens), ==, 2);
      if (distance_matches(tokens[0], expected)) {
        g_assert_cmpint(atoi(tokens[1]), ==, expected->expected_frequency);
        n_matches++;
      }
      g_strfreev(tokens);
    }
    g_assert_cmpint(n_matches, ==, 1);
  }

  g_strfreev(lines);
}

/* Integration test for Miniatures.
 * Runs the MINI command line and checks that the output CSV contains exactly
 * the expected values.
 *
 * Expected CSV lines (with header):
 *   Cache Size,Miss Ratio, Miss Ratio Byte
 *   1000,0.774473, 0.774473
 *   2000,0.764392, 0.764392
 *   5000,0.699658, 0.699658
 *   10000,0.616263, 0.616263
 */
static void test_miniatures_integration(MRCFixture *fixture,
                                        gconstpointer user_data) {
  const gchar *argv[] = {MRC_BINARY,
                         "MINI",
                         fixture->trace_path,
                         "vscsi",
                         "s3fifo",
                         "1000,2000,5000,10000",
                         "0.1",
                         fixture->csv_path,
                         "--ignore-obj-size",
                         "1",
                         NULL};
  run_mrc(argv);

  gchar **lines = read_csv_lines(fixture->csv_path);
  // Check header.
  gchar *header = g_strstrip(lines[0]);
  g_assert_cmpstr(header, ==, "Cache Size,Miss Ratio, Miss Ratio Byte");

  // Expected values.
  const int expected_cache_sizes[4] = {1000, 2000, 5000, 10000};
  const gdouble expected_miss_ratios[4] = {0.774473, 0.764392, 0.699658,
                                           0.616263};
  const gdouble expected_miss_ratios_byte[4] = {0.774473, 0.764392, 0.699658,
                                                0.616263};

  // There should be exactly 4 non-empty data lines.
  int data_line_count = 0;
  for (int i = 1; lines[i] != NULL && lines[i][0] != '\0'; i++) {
    g_assert_cmpint(data_line_count, <, 4);
    gchar **tokens = g_strsplit(lines[i], ",", 0);
    g_assert_cmpint(g_strv_length(tokens), ==, 3);

    gchar *cache_size_str = g_strstrip(tokens[0]);
    gchar *miss_ratio_str = g_strstrip(tokens[1]);
    gchar *miss_ratio_byte_str = g_strstrip(tokens[2]);

    int cache_size = atoi(cache_size_str);
    gdouble miss_ratio = g_strtod(miss_ratio_str, NULL);
    gdouble miss_ratio_byte = g_strtod(miss_ratio_byte_str, NULL);

    g_assert_cmpint(cache_size, ==, expected_cache_sizes[data_line_count]);
    g_assert_cmpfloat(fabs(miss_ratio - expected_miss_ratios[data_line_count]),
                      <, FLOAT_TOLERANCE);
    g_assert_cmpfloat(
        fabs(miss_ratio_byte - expected_miss_ratios_byte[data_line_count]), <,
        FLOAT_TOLERANCE);

    data_line_count++;
    g_strfreev(tokens);
  }
  g_assert_cmpint(data_line_count, ==, 4);

  g_strfreev(lines);
}

int main(int argc, char **argv) {
  g_test_init(&argc, &argv, NULL);

  g_test_add("/integration/test_shards_csv", MRCFixture, NULL,
             mrc_fixture_set_up, test_shards_csv_integration,
             mrc_fixture_tear_down);
  g_test_add("/integration/test_miniatures", MRCFixture, NULL,
             mrc_fixture_set_up, test_miniatures_integration,
             mrc_fixture_tear_down);

  return g_test_run();
}
