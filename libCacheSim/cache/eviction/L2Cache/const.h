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

/* where the training data (X) is generated from */
#define TRAINING_X_FROM_EVICTION 1
#define TRAINING_X_FROM_CACHE 2
#define TRAINING_DATA_SOURCE TRAINING_X_FROM_CACHE

/* where the training data (Y) is generated from */
#define TRAINING_Y_FROM_ORACLE 1
#define TRAINING_Y_FROM_ONLINE 2
#define TRAINING_Y_SOURCE TRAINING_Y_FROM_ORACLE
#define TRAINING_CONSIDER_RETAIN 1

/* training objective, regression or ranking */
#define REG 1
#define LTR 2
#define OBJECTIVE REG

//#define ACCUMULATIVE_TRAINING
//#define TRAIN_ONCE

// #define ADAPTIVE_RANK 1

/*********** exp *************/
//#define dump_ranked_seg_frac 0.05
//#undef ADAPTIVE_RANK

//#define DUMP_MODEL 1
//#define DUMP_TRAINING_DATA

/* LHD parameters */
#define HIT_PROB_MAX_AGE 86400
//#define HIT_PROB_MAX_AGE 172800
/* enable this on i5 slows down by two times */
//#define HIT_PROB_MAX_AGE 864000    /* 10 day for akamai */
#define HIT_PROB_COMPUTE_INTVL 1000000
#define LHD_EWMA 0.9

#define MAGIC 1234567890

#define DEFAULT_RANK_INTVL 20

static char *LSC_type_names[] = {"SEGCACHE", "LOGCACHE_BOTH_ORACLE", "LOGCACHE_LOG_ORACLE",
                                 "LOGCACHE_ITEM_ORACLE", "LOGCACHE_LEARNED"};

static char *obj_score_type_names[] = {"FREQ",          "FREQ_BYTE",   "FREQ_AGE",
                                       "FREQ_AGE_BYTE", "HIT_DENSITY", "ORACLE"};

static char *bucket_type_names[] = {"NO_BUCKET",        "SIZE_BUCKET",
                                    "TTL_BUCKET",       "CUSTOMER_BUCKET",
                                    "BUCKET_ID_BUCKET", "CONTENT_TYPE_BUCKET"};
