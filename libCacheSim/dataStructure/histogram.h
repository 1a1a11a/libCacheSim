#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <glib.h>  // ✅ Using Glib Hash Table

#ifndef HISTOGRAM_H
#define HISTOGRAM_H

typedef struct BinEntry {
    uint64_t frequency;   // ✅ Stores how often this distance occurs
    float threshold;      // ✅ Keeps track of the threshold for this distance
} BinEntry;

typedef struct ReuseHistogram {
    GHashTable* bins;  // ✅ Dynamic binning with a hash table
    float cold_miss_threshold;
    uint64_t cold_miss_bin;
    FILE* f;
} ReuseHistogram;

ReuseHistogram* init_histogram();
void update_histogram(ReuseHistogram* hist, uint64_t distance, float new_thres);
void export_histogram_to_csv(ReuseHistogram* hist, float rate, char* path);
void free_histogram(ReuseHistogram* hist);
void wrap_up_histogram(ReuseHistogram* hist, float rate);
uint64_t get_min_distance(ReuseHistogram* hist);
void adjust_histogram(ReuseHistogram* hist, uint64_t total_requests, float rate);

#endif
