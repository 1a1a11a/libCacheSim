/*
 * test_integration.c
 *
 * Integration tests for both SHARDS and MINI command lines.
 *
 * For SHARDS:
 *   The command run is:
 *     ../_build/bin/MRC SHARDS ../histograms/histogram_test.csv ../data/cloudPhysicsIO.vscsi vscsi 0.1
 *
 *   The output CSV (../histograms/histogram_test.csv) is expected to have the header:
 *     Distance,Frequency
 *   and data rows such as:
 *     ColdMiss,4826
 *     14829,1
 *     29659,1
 *     24099,2
 *     7419,3
 *     22249,1
 *     37079,1
 *     16689,9
 *     5569,2
 *     9279,2
 *     24109,3
 *     3719,2
 *     7429,1
 *     16699,6
 *     11139,3
 *     5579,4
 *     0,749
 *     19,287
 *     9289,4
 *     9,555
 *     31529,4
 *     ...
 *
 *   In this test we select a few rows (by line number) and check:
 *     - Row 2 (line index 1): "ColdMiss,4826"
 *     - Row 3 (line index 2): "14829,1" (numeric distance compared with ±3 tolerance)
 *     - Row 11 (line index 10): "9279,2"
 *     - Row 18 (line index 17): "0,749"
 *     - Row 21 (line index 20): "9,555"
 *     - Row 22 (line index 21): "31529,4"
 *
 * For each numeric distance we allow a difference of up to 3.
 *
 * For MINI:
 *   The command run is:
 *     ../_build/bin/MRC MINI ../data/cloudPhysicsIO.vscsi vscsi s3fifo 1000,2000,5000,10000 0.1 ../histograms-mini/histogram_test.csv --ignore-obj-size 1
 *
 *   The output CSV is compared (as in previous tests) against exact expected values.
 */

#include <glib.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include "common.h"

#define DISTANCE_TOLERANCE 3
#define FLOAT_TOLERANCE 0.00001

typedef struct {
    int line_index;
    const char* expected_distance_str; // if not NULL, compare string exactly
    int expected_distance_num;         // if expected_distance_str == NULL, use numeric comparison
    int expected_frequency;
} ExpectedRow;

/* Integration test for SHARDS.
 * Runs the SHARDS command and then checks selected rows from the output CSV.
 */
static void test_shards_csv_integration(void) {
    const char *cmd = "../_build/bin/MRC SHARDS ../histograms/histogram_test.csv ../data/cloudPhysicsIO.vscsi vscsi 0.1";
    int ret = system(cmd);
    g_assert_cmpint(ret, ==, 0);

    // Read the output CSV file.
    gchar *contents = NULL;
    gsize length = 0;
    GError *error = NULL;
    gboolean success = g_file_get_contents("../histograms/histogram_test.csv", &contents, &length, &error);
    g_assert_true(success);
    g_assert_nonnull(contents);

    // Split the file into lines.
    gchar **lines = g_strsplit(contents, "\n", -1);
    // Check header (line index 0)
    gchar *header = g_strstrip(lines[0]);
    g_assert_cmpstr(header, ==, "Distance,Frequency");

    // Define selected expected rows.
    ExpectedRow expected_rows[] = {
        {1, "ColdMiss", 0, 4826},   // Line 1: "ColdMiss,4826"
        {2, NULL, 14829, 1},         // Line 2: "14829,1"
        {10, NULL, 9279, 2},         // Line 10: "9279,2"
        {17, NULL, 0, 749},          // Line 17: "0,749"
        {20, NULL, 9, 555},          // Line 20: "9,555"
        {21, NULL, 31529, 4}         // Line 21: "31529,4"
    };
    int n_expected = sizeof(expected_rows) / sizeof(expected_rows[0]);

    for (int i = 0; i < n_expected; i++) {
        int index = expected_rows[i].line_index;
        g_assert_nonnull(lines[index]);  // ensure the line exists
        gchar *line = g_strstrip(lines[index]);
        // Split the line into two tokens.
        gchar **tokens = g_strsplit(line, ",", 0);
        int n_tokens = g_strv_length(tokens);
        g_assert_cmpint(n_tokens, ==, 2);  // Expect two columns: Distance and Frequency

        gchar *distance_token = g_strstrip(tokens[0]);
        gchar *frequency_token = g_strstrip(tokens[1]);

        // Compare frequency exactly.
        int freq = atoi(frequency_token);
        g_assert_cmpint(freq, ==, expected_rows[i].expected_frequency);

        // Compare distance.
        if (expected_rows[i].expected_distance_str != NULL) {
            // Check string equality.
            g_assert_cmpstr(distance_token, ==, expected_rows[i].expected_distance_str);
        } else {
            // Parse as integer and compare within tolerance.
            int distance_val = atoi(distance_token);
            int diff = abs(distance_val - expected_rows[i].expected_distance_num);
            g_assert_cmpint(diff, <=, DISTANCE_TOLERANCE);
        }
        g_strfreev(tokens);
    }

    g_strfreev(lines);
    g_free(contents);
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
static void test_miniatures_integration(void) {
    const char *cmd = "../_build/bin/MRC MINI ../data/cloudPhysicsIO.vscsi vscsi s3fifo 1000,2000,5000,10000 0.1 ../histograms-mini/histogram_test.csv --ignore-obj-size 1";
    int ret = system(cmd);
    g_assert_cmpint(ret, ==, 0);

    // Open the CSV file.
    gchar *contents = NULL;
    gsize length = 0;
    GError *error = NULL;
    gboolean success = g_file_get_contents("../histograms-mini/histogram_test.csv", &contents, &length, &error);
    g_assert_true(success);
    g_assert_nonnull(contents);

    // Split file into lines.
    gchar **lines = g_strsplit(contents, "\n", -1);
    // Check header.
    gchar *header = g_strstrip(lines[0]);
    g_assert_cmpstr(header, ==, "Cache Size,Miss Ratio, Miss Ratio Byte");

    // Expected values.
    const int expected_cache_sizes[4] = {1000, 2000, 5000, 10000};
    const gdouble expected_miss_ratios[4] = {0.774473, 0.764392, 0.699658, 0.616263};
    const gdouble expected_miss_ratios_byte[4] = {0.774473, 0.764392, 0.699658, 0.616263};

    // There should be exactly 4 non-empty data lines.
    int data_line_count = 0;
    for (int i = 1; lines[i] != NULL && lines[i][0] != '\0'; i++) {
        gchar **tokens = g_strsplit(lines[i], ",", 0);
        g_assert_cmpint(g_strv_length(tokens), ==, 3);

        gchar *cache_size_str = g_strstrip(tokens[0]);
        gchar *miss_ratio_str = g_strstrip(tokens[1]);
        gchar *miss_ratio_byte_str = g_strstrip(tokens[2]);

        int cache_size = atoi(cache_size_str);
        gdouble miss_ratio = g_strtod(miss_ratio_str, NULL);
        gdouble miss_ratio_byte = g_strtod(miss_ratio_byte_str, NULL);

        g_assert_cmpint(cache_size, ==, expected_cache_sizes[data_line_count]);
        g_assert_cmpfloat(fabs(miss_ratio - expected_miss_ratios[data_line_count]), <, FLOAT_TOLERANCE);
        g_assert_cmpfloat(fabs(miss_ratio_byte - expected_miss_ratios_byte[data_line_count]), <, FLOAT_TOLERANCE);

        data_line_count++;
        g_strfreev(tokens);
    }
    g_assert_cmpint(data_line_count, ==, 4);

    g_strfreev(lines);
    g_free(contents);
}

int main(int argc, char **argv) {
    g_test_init(&argc, &argv, NULL);

    g_test_add_func("/integration/test_shards_csv", test_shards_csv_integration);
    g_test_add_func("/integration/test_miniatures", test_miniatures_integration);

    return g_test_run();
}
