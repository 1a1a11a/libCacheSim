//
// Created by Juncheng Yang on 11/25/19.
//

#ifndef libCacheSim_CACHEHEADERS_H
#define libCacheSim_CACHEHEADERS_H

#ifdef __cplusplus
extern "C"
{
#endif

//#include "ARC.h"
#include "evictionAlgo/FIFO.h"
//#include "LFU.h"
//#include "LFUFast.h"
#include "evictionAlgo/LRU.h"
#include "evictionAlgo/LRUv0.h"
//#include "LRU_K.h"
#include "evictionAlgo/MRU.h"
//#include "Optimal.h"
#include "evictionAlgo/Random.h"
//#include "SLRU.h"
//#include "TTL_FIFO.h"


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
