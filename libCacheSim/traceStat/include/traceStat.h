//
// Created by Juncheng Yang on 4/20/20.
//

#ifndef libCacheSim_READERSTAT_H
#define libCacheSim_READERSTAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "../../include/libCacheSim/reader.h"
#include "../utils/include/utilsInternal.h"

guint64 get_num_of_obj(reader_t *reader);

#ifdef __cplusplus
}
#endif

#endif // libCacheSim_READERSTAT_H
