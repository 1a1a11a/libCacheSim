//
// Created by Juncheng Yang on 11/17/19.
//

#ifndef libCacheSim_H
#define libCacheSim_H

#include "config.h"
#include "libCacheSim/cache.h"
#include "libCacheSim/cacheObj.h"
#include "libCacheSim/const.h"
#include "libCacheSim/enum.h"
#include "libCacheSim/logging.h"
#include "libCacheSim/macro.h"
#include "libCacheSim/reader.h"
#include "libCacheSim/request.h"
#include "libCacheSim/sampling.h"

/* admission */
#include "libCacheSim/admissionAlgo.h"

/* eviction */
#include "libCacheSim/evictionAlgo.h"

/* sampling */
#include "libCacheSim/sampling.h"

/* cache simulator */
#include "libCacheSim/plugin.h"
#include "libCacheSim/profilerLRU.h"
#include "libCacheSim/simulator.h"

#endif  // libCacheSim_H
