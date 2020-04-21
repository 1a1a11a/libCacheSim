//
//  vscsiReader.c
//  libMimircache
//
//  Created by CloudPhysics
//  Modified by Juncheng on 5/25/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "include/vscsiReader.h"


#ifdef __cplusplus
extern "C"
{
#endif


int vscsi_setup(reader_t *const reader){
    
    vscsi_params_t* params = g_new0(vscsi_params_t, 1);
    
    reader->reader_params = params;
    
    
    /* Version 2 records are the bigger of the two */
    if ((unsigned long long)reader->base->file_size <
        (unsigned long long) sizeof(trace_v2_record_t) * MAX_TEST){
        ERROR("File too small, unable to read header.\n");
        return -1;
    }
    
    reader->base->obj_id_type = OBJ_ID_NUM;
    
    
    vscsi_version_t ver = test_vscsi_version (reader->base->mapped_file);
    switch (ver){
        case VSCSI1:
        case VSCSI2:
            reader->base->item_size = record_size(ver);
            params->vscsi_ver = ver;
            reader->base->n_total_req = reader->base->file_size / (reader->base->item_size);
            break;
            
        case UNKNOWN:
        default:
            ERROR("Trace format unrecognized.\n");
            return -1;
    }
    return 0;
}


vscsi_version_t test_vscsi_version(void *trace){
    vscsi_version_t test_buf[MAX_TEST] = {};
    
    int i;
    for (i=0; i<MAX_TEST; i++) {
        test_buf[i] = (vscsi_version_t)((((trace_v2_record_t *)trace)[i]).ver >> 8);
    }
    if (test_version(test_buf, VSCSI2))
        return(VSCSI2);
    else {
        for (i=0; i<MAX_TEST; i++) {
            test_buf[i] = (vscsi_version_t)((((trace_v1_record_t *)trace)[i]).ver >> 8);
        }
        
        if (test_version(test_buf, VSCSI1))
            return(VSCSI1);
    }
    return(UNKNOWN);
}


#ifdef __cplusplus
}
#endif
