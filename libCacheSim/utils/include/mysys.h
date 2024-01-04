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
#include <sys/resource.h>

int set_thread_affinity(pthread_t tid);

int get_n_cores(void);

int n_cores(void);

double gettime(void);

void print_cwd(void);

void create_dir(char *dir_path);

void print_glib_ver(void);

void print_resource_usage(void);

void print_rusage_diff(struct rusage *r1, struct rusage *r2);


#endif /* UTILS_h */
