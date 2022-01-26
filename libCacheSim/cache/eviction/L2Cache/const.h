#pragma once 


#define MAX_N_BUCKET 120
#define N_TRAIN_ITER 20
#define N_MAX_VALIDATION 1000
#define CACHE_STATE_UPDATE_INTVL 10

/* change to 10 will speed up */
#define N_FEATURE_TIME_WINDOW 8

#define TRAINING_X_FROM_EVICTION 1
#define TRAINING_X_FROM_CACHE    2
#define TRAINING_DATA_SOURCE    TRAINING_X_FROM_CACHE


#define TRAINING_Y_FROM_ORACLE   1
#define TRAINING_Y_FROM_ONLINE   2
//#define TRAINING_TRUTH    TRAINING_Y_FROM_ONLINE
#define TRAINING_TRUTH    TRAINING_Y_FROM_ORACLE
#define TRAINING_CONSIDER_RETAIN    1

#define REG 1
#define LTR 2
#define OBJECTIVE LTR

//#define ACCUMULATIVE_TRAINING
//#define TRAIN_ONCE

#define ADAPTIVE_RANK  1

/*********** exp *************/
//#define dump_ranked_seg_frac 0.05
//#undef ADAPTIVE_RANK


//#define DUMP_MODEL 1
//#define DUMP_TRAINING_DATA

#define HIT_PROB_MAX_AGE 86400
//#define HIT_PROB_MAX_AGE 172800
/* enable this on i5 slows down by two times */
//#define HIT_PROB_MAX_AGE 864000    /* 10 day for akamai */
#define HIT_PROB_COMPUTE_INTVL 1000000
#define LHD_EWMA 0.9

#define MAGIC 1234567890

#define DEFAULT_RANK_INTVL 20


static char *LSC_type_names[] = {
    "SEGCACHE", "SEGCACHE_ITEM_ORACLE", "SEGCACHE_SEG_ORACLE",
    "SEGCACHE_BOTH_ORACLE",
    "LOGCACHE_START_POS",
    "LOGCACHE_BOTH_ORACLE", "LOGCACHE_LOG_ORACLE", "LOGCACHE_ITEM_ORACLE",
    "LOGCACHE_LEARNED"
};

static char *obj_score_type_names[] = {
    "FREQ", "FREQ_BYTE", "FREQ_BYTE_AGE",
    "HIT_DENSITY",
    "ORACLE"
};

static char *bucket_type_names[] = {
    "NO_BUCKET",
    "SIZE_BUCKET",
    "TTL_BUCKET",
    "CUSTOMER_BUCKET",
    "BUCKET_ID_BUCKET",
    "CONTENT_TYPE_BUCKET"
};


