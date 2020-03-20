//
// Created by Juncheng Yang on 3/19/20.
//

#ifndef LIBMIMIRCACHE_CACHEOP_H
#define LIBMIMIRCACHE_CACHEOP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <glib.h>
#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>
#include <math.h>
#include "reader.h"
#include "const.h"
#include "macro.h"
#include "logging.h"
#include "utils.h"
#include "cache.h"
#include "request.h"
#include "cacheObj.h"
//#include

// same as run
gint64 warmup(cache_t *cache, reader_t* reader);

// return hit count
gint64 run(cache_t* cache, reader_t* reader);


gboolean* get_hit_results(cache_t* cache, reader_t* reader);



#endif //LIBMIMIRCACHE_CACHEOP_H
