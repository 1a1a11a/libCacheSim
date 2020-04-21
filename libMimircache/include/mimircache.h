//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef MIMIRCACHE_H
#define MIMIRCACHE_H


#ifdef __cplusplus
extern "C" {
#endif

#include "mimircache/cache.h"
#include "mimircache/request.h"
#include "mimircache/reader.h"
#include "mimircache/plugin.h"
#include "mimircache/profilerLRU.h"
#include "mimircache/profiler.h"
#include "mimircache/logging.h"


#ifdef __cplusplus
}
#include "mimircache/cacheObj.hpp"
#else
#include "mimircache/cacheObj.h"
#endif


#endif //MIMIRCACHE_H
