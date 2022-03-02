#pragma once

#define MAX_N_BUCKET 120
#define N_TRAIN_ITER 20
#define N_MAX_VALIDATION 1000
#define N_MAX_TRAINING_DATA 8192

/* L2Cache uses cache state features, so we need to track cache states 
 * and update them periodically, because there could be time when request 
 * rate is very low, we update when both conditions are met: */
// update every 10 seconds
#define CACHE_STATE_UPDATE_RINTVL 10  
// update every 1000 requests
#define CACHE_STATE_UPDATE_VINTVL 10000

/* the number of time windows we keep in features */
#define N_FEATURE_TIME_WINDOW 0

/* whether the segment utility considers retain */
#define EVICTION_CONSIDER_RETAIN 1

/* whether we normalize the pred utility by curr_seg->n_byte */
#define NORMALIZE_PRED_UTILITY 1

/* whether we use vtime instead of rtime to determine for hit probability compute interval */
#define LHD_USE_VTIME 0

/* training objective, regression or ranking */
#define REG 1
#define LTR 2
#define OBJECTIVE REG

// #define DUMP_MODEL 1
// #define DUMP_TRAINING_DATA
// #define DUMP_INFERENCE

/* if this is turned on, dump the online and offline calculated segment utility */
// #define COMPARE_TRAINING_Y 1
extern FILE *ofile_cmp_y;       // keep this even if COMPARE_TRAINING_Y is off

/* LHD parameters */
#define HIT_PROB_MAX_AGE 86400
#define HIT_PROB_CLASSES 1
//#define HIT_PROB_MAX_AGE 172800
/* enable this on i5 slows down by two times */
//#define HIT_PROB_MAX_AGE 864000    /* 10 day for akamai */
#define HIT_PROB_COMPUTE_INTVL 1000000
#define HIT_PROB_COMPUTE_INTVLR (1 << 20)
#define LHD_EWMA 0.9

// used to detect overflow
#define MAGIC 1234567890

static char *L2Cache_type_names[] = {"SEGCACHE", "LOGCACHE_BOTH_ORACLE", "LOGCACHE_LOG_ORACLE",
                                     "LOGCACHE_ITEM_ORACLE", "LOGCACHE_LEARNED"};

static char *obj_score_type_names[] = {"FREQ",          "FREQ_BYTE",   "FREQ_AGE",
                                       "FREQ_AGE_BYTE", "HIT_DENSITY", "ORACLE"};

static char *bucket_type_names[] = {"NO_BUCKET",        "SIZE_BUCKET",
                                    "TTL_BUCKET",       "CUSTOMER_BUCKET",
                                    "BUCKET_ID_BUCKET", "CONTENT_TYPE_BUCKET"};

static char *training_source_names[] = {"X_FROM_EVICTION", "X_FROM_SNAPSHOT", "Y_FROM_ONLINE",
                                        "Y_FROM_ORACLE"};