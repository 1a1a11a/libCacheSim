//
//  vscsiReader.h
//  libMimircache
//
//  Created by CloudPhysics
//  Modified by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//


#ifndef VSCSI_READER_H
#define VSCSI_READER_H



#ifdef __cplusplus
extern "C"
{
#endif

#include <stdint.h>
#include <stdio.h>
#include <stdbool.h>
#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include "../../include/mimircache.h"
#include "readerInternal.h"


#define MAX_TEST 2

typedef struct {
    uint64_t ts;
    uint32_t len;
    uint64_t lbn;
    uint16_t cmd;
} trace_item_t;

typedef struct {
    uint32_t sn;
    uint32_t len;
    uint32_t nSG;
    uint16_t cmd;
    uint16_t ver;
    uint64_t lbn;
    uint64_t ts;
} trace_v1_record_t;

typedef struct {
    uint16_t cmd;
    uint16_t ver;
    uint32_t sn;
    uint32_t len;
    uint32_t nSG;
    uint64_t lbn;
    uint64_t ts;
    uint64_t rt;
} trace_v2_record_t;


typedef enum {
    VSCSI1 = 1,
    VSCSI2 = 2,
    UNKNOWN
} vscsi_version_t;


typedef struct{
    
    vscsi_version_t vscsi_ver;
    
}vscsi_params_t;



static inline vscsi_version_t to_vscsi_version(uint16_t ver){
    return((vscsi_version_t)(ver >> 8));
}

static inline void v1_extract_trace_item(void *record, trace_item_t *trace_item){
    trace_v1_record_t *rec = (trace_v1_record_t *) record;
    trace_item->ts = rec->ts;
    trace_item->cmd = rec->cmd;
    trace_item->len = rec->len;
    trace_item->lbn = rec->lbn;
}

static inline void v2_extract_trace_item(void *record, trace_item_t *trace_item){
    trace_v2_record_t *rec = (trace_v2_record_t *) record;
    trace_item->ts = rec->ts;
    trace_item->cmd = rec->cmd;
    trace_item->len = rec->len;
    trace_item->lbn = rec->lbn;
}

static inline size_t record_size(vscsi_version_t version){
    if (version == VSCSI1)
        return(sizeof(trace_v1_record_t));
    else if (version == VSCSI2)
        return(sizeof(trace_v2_record_t));
    else
        return(-1);
}

static inline bool test_version(vscsi_version_t *test_buf,
                                vscsi_version_t version){
    int i;
    for (i=0; i<MAX_TEST; i++) {
        if (version != test_buf[i])
            return(false);
    }
    return(true);
}


static inline int vscsi_read_ver1(reader_t* reader, request_t* c){
    trace_v1_record_t *record = (trace_v1_record_t *)(reader->base->mapped_file
                                                      + reader->base->mmap_offset);
    c->real_time = record->ts;
    c->size = record->len;
    c->op = record->cmd;
    c->obj_id_ptr = GSIZE_TO_POINTER(record->lbn);
    (reader->base->mmap_offset) += reader->base->item_size;
    return 0;
}


static inline int vscsi_read_ver2(reader_t* reader, request_t* c){
    trace_v2_record_t *record = (trace_v2_record_t *)(reader->base->mapped_file
                                                      + reader->base->mmap_offset);
    c->real_time = record->ts;
    c->size = record->len;
    c->op = record->cmd;
    *((guint64*)(c->obj_id_ptr)) = record->lbn;
    (reader->base->mmap_offset) += reader->base->item_size;
    return 0;
}

static inline int vscsi_read(reader_t* reader, request_t* c){
    if (reader->base->mmap_offset >= reader->base->n_total_req
                                     * reader->base->item_size){
        c->valid = FALSE;
        return 0;
    }
    
    vscsi_params_t* params = (vscsi_params_t*) reader->reader_params;
    
    int (*fptr)(reader_t*, request_t*) = NULL;
    switch (params->vscsi_ver)
    {
        case VSCSI1:
            fptr = vscsi_read_ver1;
            break;
            
        case VSCSI2:
            fptr = vscsi_read_ver2;
            break;
        case UNKNOWN:
            ERROR("unknown vscsi version encountered\n");
            break;
    }
    return fptr(reader, c);
}


int vscsi_setup(const char *const filename, reader_t *const reader);
vscsi_version_t test_vscsi_version(void *trace);


#ifdef __cplusplus
}
#endif


#endif  /* VSCSI_READER_H */
