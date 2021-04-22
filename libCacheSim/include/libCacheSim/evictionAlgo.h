//
// Created by Juncheng Yang on 11/25/19.
//

#ifndef libCacheSim_CACHEHEADERS_H
#define libCacheSim_CACHEHEADERS_H

#ifdef __cplusplus
extern "C"
{
#endif

#include "evictionAlgo/ARC.h"
#include "evictionAlgo/FIFO.h"
#include "evictionAlgo/LFU.h"
#include "evictionAlgo/LFUDA.h"
#include "evictionAlgo/LRU.h"
#include "evictionAlgo/LRUv0.h"
#include "evictionAlgo/MRU.h"
#include "evictionAlgo/Random.h"
#include "evictionAlgo/optimal.h"
#include "evictionAlgo/LLSC.h"


#include "evictionAlgo/slabLRU.h"
#include "evictionAlgo/slabLRC.h"
#include "evictionAlgo/slabObjLRU.h"
#include "evictionAlgo/slabCommon.h"


//#include "AMP.h"
//#include "PG.h"
//#include "Mithril.h"


#ifdef __cplusplus
}
#endif




#endif //libCacheSim_CACHEHEADERS_H
