#pragma once

#define MAX_N_BUCKET 1
#define N_TRAIN_ITER 20
#define N_MAX_VALIDATION 1000
#define N_MAX_TRAINING_DATA 8192
// #define N_MAX_TRAINING_DATA 16384
#define N_INFERENCE_DATA 2048000    // large enough to not sample 

/* GLCache uses cache state features, so we need to track cache states 
 * and update them periodically, because there could be time when request 
 * rate is very low, we update when both conditions are met: */
// update every 10 seconds
#define CACHE_STATE_UPDATE_RINTVL 10  
// update every 1000 requests
#define CACHE_STATE_UPDATE_VINTVL 10000

/* the number of time windows we keep in features */
#define N_FEATURE_TIME_WINDOW 0
#define N_FEATURE_NORMAL 6

/* whether the segment utility considers retain */
#define EVICTION_CONSIDER_RETAIN 1

#define USE_DISTINCT_CUTOFF 0

/* training objective, regression or ranking */
#define REG 1
#define LTR 2
#define OBJECTIVE REG

// #define DUMP_MODEL
// #define LOAD_MODEL
// #define DUMP_TRAINING_DATA
// #define DUMP_INFERENCE_DATA
// #define DUMP_INFERENCE

#define AGE_SHIFT_FACTOR 0
// #define SCALE_AGE
#define NORMALIZE_Y
// #define RANDOMIZE_MERGE
// #define STANDARDIZE_Y
// #define BYTE_MISS_RATIO
// #define TRAIN_KEEP_HALF
// #define LOG_UTILITY


/* if this is turned on, dump the online and offline calculated segment utility */
// #define COMPARE_TRAINING_Y 1
extern FILE *ofile_cmp_y;       // keep this even if COMPARE_TRAINING_Y is off

// #define USE_LHD
/* LHD parameters */
/* whether we use vtime instead of rtime to determine for hit probability compute interval */
#define LHD_USE_VTIME 0
#define HIT_PROB_MAX_AGE 86400
#define HIT_PROB_CLASSES 1
//#define HIT_PROB_MAX_AGE 172800
/* enable this on i5 slows down by two times */
//#define HIT_PROB_MAX_AGE 864000    /* 10 day for akamai */
#define HIT_PROB_COMPUTE_INTVL 1000000
#define HIT_PROB_COMPUTE_INTVLR 86400
#define LHD_EWMA 0.9

// used to detect overflow
#define MAGIC 1234567890

static char *GLCache_type_names[] = {"SEGCACHE", "LOGCACHE_BOTH_ORACLE", "LOGCACHE_LOG_ORACLE",
                                     "LOGCACHE_ITEM_ORACLE", "LOGCACHE_LEARNED"};

static char *obj_score_type_names[] = {"FREQ",          "FREQ_BYTE",   "FREQ_AGE",
                                       "FREQ_AGE_BYTE", "HIT_DENSITY", "ORACLE"};

static char *bucket_type_names[] = {"NO_BUCKET",        "SIZE_BUCKET",
                                    "TTL_BUCKET",       "CUSTOMER_BUCKET",
                                    "BUCKET_ID_BUCKET", "CONTENT_TYPE_BUCKET"};

static char *training_source_names[] = {"X_FROM_EVICTION", "X_FROM_SNAPSHOT", "Y_FROM_ONLINE",
                                        "Y_FROM_ORACLE"};