#pragma once

#define MAX_N_BUCKET 120
#define N_TRAIN_ITER 20
#define N_MAX_VALIDATION 1000

/* L2Cache uses cache state features, so we need to track cache states 
 * and update them periodically, because there could be time when request 
 * rate is very low, we update when both conditions are met: */
#define CACHE_STATE_UPDATE_RINTVL 10  // update every 10 seconds
#define CACHE_STATE_UPDATE_VINTVL 1000// update every 10000 requests

/* change to 10 will slow down */
/* the number of time windows we keep in features */
#define N_FEATURE_TIME_WINDOW 8

/* whether the training data (segment utility) considers retain */
#define TRAINING_CONSIDER_RETAIN 1

/* training objective, regression or ranking */
#define REG 1
#define LTR 2
#define OBJECTIVE REG

// #define DUMP_MODEL 1
//#define DUMP_TRAINING_DATA

/* if this is turned on, dump the online and offline calculated segment utility */
#define COMPARE_TRAINING_Y
#define COMPARE_TRAINING_Y_FILE "compare_y.txt"
extern FILE *ofile_cmp_y;

/* LHD parameters */
#define HIT_PROB_MAX_AGE 86400
//#define HIT_PROB_MAX_AGE 172800
/* enable this on i5 slows down by two times */
//#define HIT_PROB_MAX_AGE 864000    /* 10 day for akamai */
#define HIT_PROB_COMPUTE_INTVL 1000000
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
