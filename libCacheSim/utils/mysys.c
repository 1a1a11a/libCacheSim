//
//  utils.c
//  libCacheSim
//
//  Created by Juncheng on 6/2/16.
//  Copyright © 2016 Juncheng. All rights reserved.
//

#define _GNU_SOURCE

#include <dirent.h>
#include <glib.h>
#include <math.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef __APPLE__
#include <sys/sysinfo.h>
#endif
#include "../include/config.h"
#include "../include/libCacheSim/const.h"
#include "../include/libCacheSim/logging.h"
#include "include/mysys.h"
#include "include/mytime.h"

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

  // INFO(
  //     "This system has %d processors configured and "
  //     "%d processors available.\n",
  //     get_nprocs_conf(), get_nprocs());

  return get_nprocs();
#else
  return sysconf(_SC_NPROCESSORS_ONLN);
#endif
  WARN("Unknown system, use 4 threads as default\n");
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

// unsigned int n_cores() {
//   unsigned int eax = 11, ebx = 0, ecx = 1, edx = 0;

//   asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) :
//   "0"(eax), "2"(ecx) :);

//   printf("Cores: %d\nThreads: %d\nActual thread: %d\n", eax, ebx, edx);
//   return ebx;
// }

int n_cores(void) { return get_n_cores(); }

double gettime(void) {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

// void create_dir(char *dir_path) {
//   if (access(dir_path, F_OK) == -1) {
//     mkdir(dir_path, 0777);
//   }
// }

void create_dir(char *dir_path) {
    char temp[1024];
    snprintf(temp, sizeof(temp), "%s", dir_path);
    size_t len = strlen(temp);
    if (temp[len - 1] == '/') {
        temp[len - 1] = '\0';
    }
    for (char *p = temp + 1; *p; p++) {
        if (*p == '/') {
            *p = '\0';
            if (access(temp, F_OK) == -1) {
                if (mkdir(temp, 0777) == -1) {
                    perror("mkdir error");
                }
            }
            *p = '/';
        }
    }
    if (access(temp, F_OK) == -1) {
        if (mkdir(temp, 0777) == -1) {
            perror("mkdir error");
        }
    }
}

void get_resouce_usage(struct rusage *r_usage) {
  getrusage(RUSAGE_SELF, r_usage);
}

void print_resource_usage(void) {
  struct rusage r_usage;
  getrusage(RUSAGE_SELF, &r_usage);

  printf("CPU user time %lld us, sys time %lld us\n",
         TIMEVAL_TO_USEC(r_usage.ru_utime), TIMEVAL_TO_USEC(r_usage.ru_stime));
  printf(
      "Mem RSS %.2lf MB, soft page fault %ld - hard page fault %ld, block "
      "input %ld - block output %ld, voluntary context switches %ld - "
      "involuntary %ld\n",
      (double)r_usage.ru_maxrss / (1024.0), r_usage.ru_minflt,
      r_usage.ru_majflt, r_usage.ru_inblock, r_usage.ru_oublock,
      r_usage.ru_nvcsw, r_usage.ru_nivcsw);
}

void print_rusage_diff(struct rusage *r1, struct rusage *r2) {
  printf("******  CPU user time %.2lf s, sys time %.2lf s\n",
         (TIMEVAL_TO_SEC(r2->ru_utime) - TIMEVAL_TO_SEC(r1->ru_utime)),
         (TIMEVAL_TO_SEC(r2->ru_stime) - TIMEVAL_TO_SEC(r1->ru_stime)));

  printf(
      "******  Mem RSS %.2lf MB, soft page fault %ld - hard page fault %ld, "
      "voluntary context switches %ld - involuntary %ld\n",
      (double)(r2->ru_maxrss - r1->ru_maxrss) / (1024.0),
      (r2->ru_minflt - r1->ru_minflt), (r2->ru_majflt - r1->ru_majflt),
      (r2->ru_nvcsw - r1->ru_nvcsw), (r2->ru_nivcsw - r1->ru_nivcsw));
}
