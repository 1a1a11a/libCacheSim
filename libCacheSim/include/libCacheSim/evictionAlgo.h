#pragma once

#include "evictionAlgo/ARC.h"
#include "evictionAlgo/ARC2.h"
#include "evictionAlgo/Clock.h"
#include "evictionAlgo/FIFO.h"
#include "evictionAlgo/FIFOMerge.h"
#include "evictionAlgo/LFU.h"
#include "evictionAlgo/LFUDA.h"
#include "evictionAlgo/LRU.h"
#include "evictionAlgo/LRUv0.h"
#include "evictionAlgo/MRU.h"
#include "evictionAlgo/Random.h"
// #include "evictionAlgo/MClock.h"

#include "evictionAlgo/Belady.h"
#include "evictionAlgo/BeladySize.h"
#include "evictionAlgo/CR_LFU.h"
#include "evictionAlgo/Cacheus.h"
#include "evictionAlgo/GDSF.h"
#include "evictionAlgo/Hyperbolic.h"
#include "evictionAlgo/LFU.h"
#include "evictionAlgo/LHD.h"
#include "evictionAlgo/LeCaR.h"
#include "evictionAlgo/LeCaRv0.h"
#include "evictionAlgo/SLRU.h"
#include "evictionAlgo/SR_LRU.h"

#if defined(ENABLE_L2CACHE) && ENABLE_L2CACHE == 1
#include "evictionAlgo/L2Cache.h"
#endif
