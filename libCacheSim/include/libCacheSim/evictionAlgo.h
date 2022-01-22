#pragma once


#include "evictionAlgo/ARC.h"
#include "evictionAlgo/FIFO.h"
#include "evictionAlgo/LFUFast.h"
#include "evictionAlgo/LFUDA.h"
#include "evictionAlgo/LRU.h"
#include "evictionAlgo/LRUv0.h"
#include "evictionAlgo/MRU.h"
#include "evictionAlgo/Random.h"
#include "evictionAlgo/Clock.h"

#include "evictionAlgo/LFU.h"
#include "evictionAlgo/GDSF.h"

#include "evictionAlgo/LHD.h"
#include "evictionAlgo/Hyperbolic.h"
#include "evictionAlgo/LeCaR.h"
#include "evictionAlgo/Cacheus.h"

#include "evictionAlgo/Optimal.h"
#include "evictionAlgo/OptimalSize.h"

#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
#include "evictionAlgo/L2Cache.h"
#endif


#include "evictionAlgo/slabLRU.h"
#include "evictionAlgo/slabLRC.h"
#include "evictionAlgo/slabObjLRU.h"
#include "evictionAlgo/slabCommon.h"


//#include "AMP.h"
//#include "PG.h"
//#include "Mithril.h"

