//
//  profilerLRU.h
//  profilerLRU
//
//  Created by Juncheng on 5/24/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef profilerLRU_h
#define profilerLRU_h

#include <glib.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#include "const.h"
#include "dist.h"
#include "reader.h"

#ifdef __cplusplus
extern "C" {
#endif

double *get_lru_obj_miss_ratio(reader_t *reader, gint64 size);
double *get_lru_obj_miss_ratio_curve(reader_t *reader, gint64 size);

/* not possible because it requires huge array for storing reuse_hit_cnt
 * it is possible to implement this in O(NlogN) however, we need to modify splay
 * tree
 * TODO(Jason): maybe we want to add it
 * */
// double *get_lru_byte_miss_ratio(reader_t* reader, gint64 size);

/* internal use, can be used externally, but not recommended */
guint64 *_get_lru_miss_cnt(reader_t *reader, gint64 size);

#ifdef __cplusplus
}
#endif

#endif /* profilerLRU_h */
