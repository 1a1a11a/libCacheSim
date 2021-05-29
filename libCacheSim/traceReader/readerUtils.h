//
// Created by Juncheng Yang on 5/7/20.
//

#ifndef libCacheSim_READERUTILS_H
#define libCacheSim_READERUTILS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "../include/libCacheSim/reader.h"
#include "../include/libCacheSim/request.h"

void read_first_req(reader_t *reader, request_t *req);


void read_last_req(reader_t *reader, request_t *req);


/**
 * given a window, generate an array specifying the boundary splitting windows (real time),
 * the boundary is specified using the index in request sequence
 * @param reader
 * @param window
 * @return
 */
int64_t *get_window_boundary(reader_t *reader, uint32_t window, int32_t *n_window);


#ifdef __cplusplus
}
#endif

#endif //libCacheSim_READERUTILS_H
