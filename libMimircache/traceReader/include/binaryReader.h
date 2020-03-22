//
//  binaryReader.h
//  libMimircache
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


#include "../../include/mimircache.h"
#include "readerInternal.h"


typedef struct {
  gint obj_id_pos;                  // the beginning bytes in the struct
  guint obj_id_len;                  // the size of obj_id
  char obj_id_type;

  gint op_pos;
  guint op_len;
  char op_type;

  gint real_time_pos;
  guint real_time_len;
  char real_time_type;

  gint size_pos;
  guint size_len;
  char size_type;

  gint unused_pos1;
  guint unused_len1;
  char unused_type1;

  gint unused_pos2;
  guint unused_len2;
  char unused_type2;

  char fmt[MAX_BIN_FMT_STR_LEN];
  guint num_of_fields;
} binary_params_t;

binary_init_params_t *new_binaryReader_init_params(gint label_pos,
                                                   gint op_pos,
                                                   gint real_time_pos,
                                                   gint size_pos,
                                                   const char *fmt);

/* binary extract extracts the attribute from record, at given pos */
static inline void binary_extract(char *record, int pos, int len,
                                  char type, void *written_to) {

  ;

  switch (type) {
    case 'b':
    case 'B':
    case 'c':
    case '?': WARNING("given obj_id_type %c cannot be used for obj_id or time\n", type);
      break;

    case 'h':
//            *(gint16*)written_to = *(gint16*)(record + pos);
      *(gint64 *) written_to = *(gint16 *) (record + pos);
      break;
    case 'H':*(gint64 *) written_to = *(guint16 *) (record + pos);
      break;

    case 'i':
    case 'l':*(gint64 *) written_to = *(gint32 *) (record + pos);
      break;

    case 'I':
    case 'L':*(gint64 *) written_to = *(guint32 *) (record + pos);
      break;

    case 'q':*(gint64 *) written_to = *(gint64 *) (record + pos);
      break;
    case 'Q':*(gint64 *) written_to = *(guint64 *) (record + pos);
      break;

    case 'f':*(float *) written_to = *(float *) (record + pos);
      *(double *) written_to = *(float *) (record + pos);
      break;

    case 'd':*(double *) written_to = *(double *) (record + pos);
      break;

    case 's':strncpy((char *) written_to, (char *) (record + pos), len);
      ((char *) written_to)[len] = 0;
      break;

    default: ERROR("DO NOT recognize given format character: %c\n", type);
      break;
  }
}

static inline int binary_read(reader_t *reader, request_t *req) {
  if (reader->base->mmap_offset >= reader->base->n_total_req * reader->base->item_size) {
    req->valid = FALSE;
    return 0;
  }

  binary_params_t *params = (binary_params_t *) reader->reader_params;

  char *record = (reader->base->mapped_file + reader->base->mmap_offset);
  if (params->obj_id_type) {
    binary_extract(record, params->obj_id_pos, params->obj_id_len,
                   params->obj_id_type, &(req->obj_id_ptr));
  }
  if (params->real_time_type) {
    binary_extract(record, params->real_time_pos, params->real_time_len,
                   params->real_time_type, &(req->real_time));
  }
  if (params->size_type) {
    binary_extract(record, params->size_pos, params->size_len,
                   params->size_type, &(req->size));
  }
  if (params->op_type) {
        binary_extract(record, params->op_pos, params->op_len,
                       params->op_type, &(req->op));
  }

  if (params->unused_type1) {
    binary_extract(record, params->unused_pos1, params->unused_len1,
                   params->unused_type1, &(req->extra_data));
  }

  (reader->base->mmap_offset) += reader->base->item_size;
  return 0;
}

/* function to setup binary reader */
int binaryReader_setup(const char *const filename,
                       reader_t *const reader,
                       const binary_init_params_t *const params);

#ifdef __cplusplus
}
#endif

#endif
