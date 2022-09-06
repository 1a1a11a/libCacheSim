//
//  utils.h
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#ifndef UTILS_h
#define UTILS_h

#include <pthread.h>

int set_thread_affinity(pthread_t tid);

int get_n_cores(void);

void print_cwd(void);

void print_glib_ver(void);

#endif /* UTILS_h */
