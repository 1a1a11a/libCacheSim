#pragma once


static inline unsigned int n_cores() {
  unsigned int eax = 11, ebx = 0, ecx = 1, edx = 0;

  asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "0"(eax), "2"(ecx) :);

  //  printf("Cores: %d\nThreads: %d\nActual thread: %d\n", eax, ebx, edx);
  return ebx;
}


static inline double gettime() {
  struct timeval tv;
  gettimeofday(&tv, NULL);

  return tv.tv_sec + tv.tv_usec / 1000000.0;
}
