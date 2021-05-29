//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef libCacheSim_H
#define libCacheSim_H

#ifdef __cplusplus
extern "C" {
#endif

#include "config.h"
#include "libCacheSim/logging.h"
#include "libCacheSim/macro.h"
#include "libCacheSim/const.h"

#include "libCacheSim/enum.h"
#include "libCacheSim/struct.h"

#include "libCacheSim/cache.h"
#include "libCacheSim/reader.h"
#include "libCacheSim/request.h"
#include "libCacheSim/cacheObj.h"

#include "libCacheSim/sampler.h"

/* eviction */
#include "libCacheSim/evictionAlgo.h"

/* sampling */
#include "libCacheSim/sampler.h"

/* cache simulator */
#include "libCacheSim/profilerLRU.h"
#include "libCacheSim/simulator.h"


#ifdef __cplusplus
}
#endif

#endif // libCacheSim_H
