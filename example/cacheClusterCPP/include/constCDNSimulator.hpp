//
//  constCDNSimulator.cpp
//  CDNSimulator
//
//  Created by Juncheng on 7/11/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//


#ifndef constCDNSimulator_HPP
#define constCDNSimulator_HPP

#ifdef __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <glib.h>

#include "csvReader.h"
#include "binaryReader.h"

#ifdef __cplusplus
}
#endif

enum objType {
  full_obj = 1,
  chunk_obj = 2,
  unknown = 3
};

typedef enum {
  akamai1b,
  akamai1bWithHost,
  akamai1bWithBucket
} trace_type_t;

// #define FIND_REP_COEF
//#define FIND_REP_COEF_AGE
//#define FIND_REP_COEF_POPULARITY
//#define OUTPUT_HIT_RESULT
//#define TRACK_EXCLUSIVE_HIT

//#define TRACK_FAILURE_IMPACTED_HIT_START 200000000

#define N_BUCKET 100
//#define TRACK_BUCKET_STAT

//#define DEBUG_UNAVAILABILITY


// label_column, op_column, real_time_column, size_column, has_header, delimiter, traceID_column;
#define OLD_AKAMAI1_CSV_PARAM_INIT (new_csvReader_init_params(5, -1, 1, -1, FALSE, '\t', -1))
#define OLD_AKAMAI3_CSV_PARAM_INIT (new_csvReader_init_params(6, -1, 1, -1, FALSE, '\t', -1))
#define OLD_AKAMAI0_CSV_PARAM_INIT (new_csvReader_init_params(2, -1, 1, -1, FALSE, '\t', -1))
#define WIKI_CSV_PARAM_INIT (new_csvReader_init_params(3, -1, 2, -1, FALSE, ',', -1))
#define PLAIN_TXT_PARAM_INIT (new_csvReader_init_params(1, -1, -1, -1, FALSE, ',', -1))
#define WIKI2_CSV_PARAM_INIT (new_csvReader_init_params(2, -1, -1, 3, FALSE, ' ', -1))
#define AKAMAI1_CSV_PARAM_INIT (new_csvReader_init_params(2, -1, 1, 3, FALSE, ' ', -1))

// label_pos, op_pos, realtime_pos, size_pos, format_string
#define WIKI2_BINARY_PARAM_INIT ( new_binaryReader_init_params(1, -1, -1, 2, "LL") )
#define AKAMAI1_BINARY_PARAM_INIT ( new_binaryReader_init_params(2, -1, 1, 3, "III") )



#endif /* constCDNSimulator_HPP */

