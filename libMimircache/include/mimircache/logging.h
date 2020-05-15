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


#define NORMAL  "\x1B[0m"
#define RED     "\x1B[31m"
#define GREEN   "\x1B[32m"
#define YELLOW  "\x1B[33m"
#define BLUE    "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN    "\x1B[36m"

#define DEBUG3_LEVEL 4
#define DEBUG2_LEVEL 5
#define DEBUG_LEVEL   6
//#define VERBOSE_LEVEL 2
#define INFO_LEVEL    7
#define WARNING_LEVEL 8
#define SEVERE_LEVEL  9


int log_header(int level, const char *file, int line);
//void log_lock(int);
void print_stack_trace(void);


#define LOGGING(level, FMT, ...) { \
    log_lock(1); \
    if(log_header(level, __FILE__, __LINE__)) { \
        printf(FMT, ##__VA_ARGS__); printf("%s", NORMAL); fflush(stdout); \
    } \
    log_lock(0); \
}

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
 * convert size to an appropriate string with unit, for example 1048576 will be 1MB
 * @param size
 * @param str a 8 byte char array
 */
static inline void convert_size_to_str(long long size, char *str){

  if (size >= 1024L*1024*1024*1024) {
    sprintf(str, "%.0lfTB", (double)size/(1024L*1024*1024*1024));
  } else if (size >= 1024L*1024*1024) {
    sprintf(str, "%.0lf GB", (double)size/(1024L*1024*1024));
  } else if (size >= 1024L*1024) {
    sprintf(str, "%.0lf MB", (double)size/(1024L*1024));
  } else if (size >= 1024L) {
    sprintf(str, "%.0lf KB", (double)size/(1024L));
  } else {
    sprintf(str, "%lld", size);
  }
}



#endif
