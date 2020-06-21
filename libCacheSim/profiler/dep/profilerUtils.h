//
//  profilerUtils.h
//  profilerUtils
//
//  Created by Juncheng on 2/24/18.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef profilerUtils_h
#define profilerUtils_h

#include "glib.h"
#include "reader.h"
#include "const.h"
#include "request.h"


#ifdef __cplusplus
extern "C"
{
#endif

double get_log_base(guint64 max, guint64 expect_result);

GSList *get_last_access_dist_seq(reader_t *reader,
                                 void (*funcPtr)(reader_t *, request_t *));

GArray *get_bp_vtime(reader_t *reader,
                     gint64 time_interval,
                     gint64 num_of_pixels);


GArray *get_bp_rtime(reader_t *reader,
                     gint64 time_interval,
                     gint64 num_of_pixels);


#ifdef __cplusplus
}
#endif


#endif
