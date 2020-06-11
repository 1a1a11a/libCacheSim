#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <execinfo.h>
#include "../config.h"


int log_header(int level, const char *file, int line);
void print_stack_trace(void);


#define LOGGING(level, FMT, ...) do { \
    log_lock(1); \
    if(log_header(level, __FILE__, __LINE__)) { \
        printf(FMT, ##__VA_ARGS__); printf("%s", NORMAL); fflush(stdout); \
    } \
    log_lock(0); \
} while (0)

#if MIMIR_LOGLEVEL <= DEBUG3_LEVEL
#define DEBUG3(FMT, ...) LOGGING(DEBUG3_LEVEL, FMT, ##__VA_ARGS__)
#else
#define DEBUG3(FMT, ...)
#endif

#if MIMIR_LOGLEVEL <= DEBUG2_LEVEL
    #define DEBUG2(FMT, ...) LOGGING(DEBUG2_LEVEL, FMT, ##__VA_ARGS__)
#else
    #define DEBUG2(FMT, ...)
#endif

#if MIMIR_LOGLEVEL <= DEBUG_LEVEL
    #define DEBUG(FMT, ...) LOGGING(DEBUG_LEVEL, FMT, ##__VA_ARGS__)
#else
    #define DEBUG(FMT, ...)
#endif

#if MIMIR_LOGLEVEL <= INFO_LEVEL
    #define INFO(FMT, ...) LOGGING(INFO_LEVEL, FMT, ##__VA_ARGS__)
#else
    #define INFO(FMT, ...)
#endif

#if MIMIR_LOGLEVEL <= WARNING_LEVEL
    #define WARNING(FMT, ...) LOGGING(WARNING_LEVEL, FMT, ##__VA_ARGS__)
#else
    #define WARNING(FMT, ...)
#endif

#if MIMIR_LOGLEVEL <= SEVERE_LEVEL
    #define ERROR(FMT, ...) LOGGING(SEVERE_LEVEL, FMT, ##__VA_ARGS__)
#else
    #define ERROR(FMT, ...)
#endif


static inline void log_lock(int lock)
{
  // static std::mutex m;
  static pthread_mutex_t mtx = PTHREAD_MUTEX_INITIALIZER;
  if (lock) {
    pthread_mutex_lock(&mtx);
    // m.lock();
  } else {
    pthread_mutex_unlock(&mtx);
    // m.unlock();
  }
}


/**
 * convert size to an appropriate string with unit, for example 1048576 will be 1 MiB
 * @param size
 * @param str a 8 byte char array
 */
static inline void convert_size_to_str(long long size, char *str){

  if (size >= 1024L*1024*1024*1024) {
    sprintf(str, "%.0lf TiB", (double)size/(1024L*1024*1024*1024));
  } else if (size >= 1024L*1024*1024) {
    sprintf(str, "%.0lf GiB", (double)size/(1024L*1024*1024));
  } else if (size >= 1024L*1024) {
    sprintf(str, "%.0lf MiB", (double)size/(1024L*1024));
  } else if (size >= 1024L) {
    sprintf(str, "%.0lf KiB", (double)size/(1024L));
  } else {
    sprintf(str, "%lld", size);
  }
}



#endif
