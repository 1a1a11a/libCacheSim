//
//  binaryReader.h
//  libCacheSim
//
//  Created by Juncheng on 2/28/17.
//  Copyright © 2017 Juncheng. All rights reserved.
//



#ifndef _BINARY_READER_
#define _BINARY_READER_

#ifdef __cplusplus
extern "C"
{
#endif

#include "../../include/libCacheSim.h"

typedef struct {
  gint obj_id_field;                  // the beginning bytes in the struct
  guint obj_id_len;                   // the size of obj_id
  char obj_id_type;

  gint op_field;
  guint op_len;
  char op_type;

  gint real_time_field;
  guint real_time_len;
  char real_time_type;

  gint obj_size_field;
  guint obj_size_len;
  char obj_size_type;

  gint ttl_field;
  guint ttl_len;
  char ttl_type;

  gint unused_field1;
  guint unused_len1;
  char unused_type1;

  gint unused_field2;
  guint unused_len2;
  char unused_type2;

  char fmt[MAX_BIN_FMT_STR_LEN];
  guint num_of_fields;
} binary_params_t;

int binary_read_one_req(reader_t *reader, request_t *req);

/* function to setup binary reader */
int binaryReader_setup(reader_t *const reader);

#ifdef __cplusplus
}
#endif

#endif
