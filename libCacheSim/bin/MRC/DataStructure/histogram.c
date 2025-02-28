
#include "histogram.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

ReuseHistogram* init_histogram() {
  ReuseHistogram* hist = (ReuseHistogram*)malloc(sizeof(ReuseHistogram));
  hist->bins = g_hash_table_new(g_int64_hash, g_int64_equal);  // ✅ Dynamic binning
  hist->cold_miss_bin = 0;
  hist->cold_miss_threshold = 0.0;  // ✅ Keeps existing logic
  hist->f = fopen("/users/Claire/libCacheSim/histogram.log", "w");
  return hist;
}

void update_histogram(ReuseHistogram* hist, uint64_t distance, float new_thres) {
  if (distance == (uint64_t)-1) {  // ✅ Cold Miss Handling (Unchanged)
    if (hist->cold_miss_threshold > new_thres) {
      hist->cold_miss_bin = (uint64_t)(hist->cold_miss_bin * new_thres / hist->cold_miss_threshold);
      hist->cold_miss_threshold = new_thres;
    }
    if (hist->cold_miss_threshold == 0) {
      hist->cold_miss_threshold = new_thres;
    }
    hist->cold_miss_bin++;
    return;
  }

  // ✅ Check if the distance exists in the hash table
  BinEntry* bin = (BinEntry*)g_hash_table_lookup(hist->bins, &distance);

  if (bin) {
    // ✅ Distance already exists, update frequency and threshold

    if (bin->threshold > new_thres) {
      bin->frequency *= new_thres / bin->threshold;
      bin->threshold = new_thres;  // ✅ Maintain the lowest threshold
    }
    bin->frequency++;
  } else {
    // fprintf(hist->f,"New Distance: %lu, Final Number:%lu\n", distance,    (uint64_t)((double)(distance) /(double)
    // new_thres)); ✅ Distance does not exist, create a new bin
    BinEntry* new_bin = (BinEntry*)malloc(sizeof(BinEntry));
    new_bin->frequency = 1;
    new_bin->threshold = new_thres;
    g_hash_table_insert(hist->bins, g_memdup2(&distance, sizeof(uint64_t)), new_bin);
  }
}

void wrap_up_histogram(ReuseHistogram* hist, float rate) {
  // ✅ Iterate over hash table bins
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, hist->bins);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    uint64_t distance = *(uint64_t*)key;
    BinEntry* bin = (BinEntry*)value;
    bin->frequency = (uint64_t)(bin->frequency * rate / bin->threshold);
  }
  hist->cold_miss_bin = (uint64_t)(hist->cold_miss_bin * rate / hist->cold_miss_threshold);
}

void export_histogram_to_csv(ReuseHistogram* hist, float rate, char* path) {
  printf("Histogram path: %s\n", path);

  FILE* file = fopen(path, "w");
  if (!file) return;

  fprintf(file, "Distance,Frequency\n");

  // ✅ Export cold misses first
  if (hist->cold_miss_bin > 0) {
    fprintf(file, "ColdMiss,%lu\n", hist->cold_miss_bin);
  }

  // ✅ Iterate over hash table bins
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, hist->bins);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    uint64_t distance = *(uint64_t*)key;
    BinEntry* bin = (BinEntry*)value;
    double scaled_distance = (double)(distance) / (double)rate;

    // ✅ Ensure result is within uint64_t bounds before conversion
    if (scaled_distance > (double)UINT64_MAX) {
      fprintf(file, "Overflow,%lu\n", bin->frequency);
    } else {
      fprintf(file, "%lu,%lu\n", (uint64_t)scaled_distance, bin->frequency);
    }
  }

  fclose(file);
}

uint64_t get_min_distance(ReuseHistogram* hist) {
  if (!hist || g_hash_table_size(hist->bins) == 0) {
    return UINT64_MAX;  // Return max value if histogram is empty
  }

  GHashTableIter iter;
  gpointer key, value;
  uint64_t min_distance = UINT64_MAX;

  g_hash_table_iter_init(&iter, hist->bins);
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    uint64_t distance = *(uint64_t*)key;
    if (distance < min_distance) {
      min_distance = distance;
    }
  }

  return min_distance;
}

void free_histogram(ReuseHistogram* hist) {
  GHashTableIter iter;
  gpointer key, value;
  g_hash_table_iter_init(&iter, hist->bins);

  // ✅ Free all BinEntry structures
  while (g_hash_table_iter_next(&iter, &key, &value)) {
    free(value);
  }

  g_hash_table_destroy(hist->bins);  // ✅ Free hash table memory
  free(hist);
}
