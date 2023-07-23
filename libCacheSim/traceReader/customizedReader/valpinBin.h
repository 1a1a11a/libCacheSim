#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include "../../include/libCacheSim/reader.h"
#include "binaryUtils.h"

static inline int valpinReader_setup(reader_t *reader)
{
    reader->trace_type = VALPIN_TRACE;
    reader->trace_format = BINARY_TRACE_FORMAT;
    reader->item_size = 8;
    reader->n_total_req = (uint64_t)reader->file_size / (reader->item_size);
    reader -> obj_id_is_num = true;
    return 0;
}

static inline int valpin_read_one_req(reader_t *reader, request_t *req)
{
    char *record = read_bytes(reader);
    if (record == NULL) {
        req->valid = FALSE;
        return 1;
    }

    req->obj_id = *(uint64_t *)record;
    req->obj_size = 1;

    return 0;

}



#ifdef __cplusplus
}
#endif