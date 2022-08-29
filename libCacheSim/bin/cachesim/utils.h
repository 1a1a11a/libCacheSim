#pragma once

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <stdio.h>

// static inline unsigned int n_cores() {
//   unsigned int eax = 11, ebx = 0, ecx = 1, edx = 0;

//   asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "0"(eax), "2"(ecx) :);

//   printf("Cores: %d\nThreads: %d\nActual thread: %d\n", eax, ebx, edx);
//   return ebx;
// }

static inline long n_cores() {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

static inline double gettime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return tv.tv_sec + tv.tv_usec / 1000000.0;
}

static inline void create_dir(char *dir_path) {
  if (access(dir_path, F_OK) == -1) {
    mkdir(dir_path, 0777);
  }
}
