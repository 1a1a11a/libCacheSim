//
//  utils.h
//  libMimircache
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef UTILS_h
#define UTILS_h

#include "../../include/mimircache/const.h"
#include "../../include/mimircache/logging.h"

#include <stdio.h>
#include <math.h>
#include <glib.h>
#include <pthread.h>
#include <unistd.h>

#include <sched.h>
#ifndef __APPLE__
#include <sys/sysinfo.h>
#endif


int set_thread_affinity(pthread_t tid);

guint get_n_cores(void);



#endif /* UTILS_h */
