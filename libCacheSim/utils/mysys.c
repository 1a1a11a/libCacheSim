//
//  utils.c
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#include "include/mysys.h"

int set_thread_affinity(pthread_t tid) {
#ifdef __linux__
  static int last_core_id = -1;
  int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

  last_core_id++;
  last_core_id %= num_cores;
  DEBUG("assign thread affinity %d/%d\n", last_core_id, num_cores);

  cpu_set_t cpuset;
  CPU_ZERO(&cpuset);
  CPU_SET(last_core_id, &cpuset);

  int rc = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
  if (rc != 0) {
    WARN("Error calling pthread_setaffinity_np: %d\n", rc);
  }
#endif
  return 0;
}

int get_n_cores(void) {
#ifdef __linux__

  INFO("This system has %d processors configured and "
       "%d processors available.\n",
       get_nprocs_conf(), get_nprocs());

  return get_nprocs();
#else
  WARN("non linux system, use 4 threads as default\n");
#endif
  return 4;
}

void print_cwd(void) {
  char cwd[1024];
  if (getcwd(cwd, sizeof(cwd)) != NULL) {
    printf("Current working dir: %s\n", cwd);
  } else {
    perror("getcwd() error");
  }
}

void print_glib_ver(void) {
  printf("glib version %d.%d.%d %d\n", glib_major_version, glib_minor_version,
         glib_micro_version, glib_binary_age);
}

