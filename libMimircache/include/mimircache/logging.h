#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <execinfo.h>


#define NORMAL  "\x1B[0m"
#define RED     "\x1B[31m"
#define GREEN   "\x1B[32m"
#define YELLOW  "\x1B[33m"
#define BLUE    "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN    "\x1B[36m"

#define DEBUG_LEVEL   1
#define VERBOSE_LEVEL 2
#define INFO_LEVEL    3
#define WARNING_LEVEL 4
#define SEVERE_LEVEL  5

#ifndef LOGLEVEL
//#define LOGLEVEL DEBUG_LEVEL
#define LOGLEVEL INFO_LEVEL
#endif


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

#if LOGLEVEL <= VERBOSE_LEVEL
    #define VERBOSE(FMT, ...) LOGGING(VERBOSE_LEVEL, FMT, ##__VA_ARGS__)
#else
    #define VERBOSE(FMT, ...)
#endif

#if LOGLEVEL <= DEBUG_LEVEL
    #define DEBUG(FMT, ...) LOGGING(DEBUG_LEVEL, FMT, ##__VA_ARGS__)
#else
    #define DEBUG(FMT, ...)
#endif

#if LOGLEVEL <= INFO_LEVEL
    #define INFO(FMT, ...) LOGGING(INFO_LEVEL, FMT, ##__VA_ARGS__)
#else
    #define INFO(FMT, ...)
#endif

#if LOGLEVEL <= WARNING_LEVEL
    #define WARNING(FMT, ...) LOGGING(WARNING_LEVEL, FMT, ##__VA_ARGS__)
#else
    #define WARNING(FMT, ...)
#endif

#if LOGLEVEL <= SEVERE_LEVEL
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

#endif
