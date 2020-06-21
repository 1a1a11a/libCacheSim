#ifndef LOGGING_H
#define LOGGING_H

#include "../config.h"
#include "const.h"
#include <execinfo.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

static inline int _log_header(int level, const char *file, int line);
static inline void print_stack_trace(void);

#define LOGGING(level, FMT, ...)                                               \
  do {                                                                         \
    log_lock(1);                                                               \
    if (_log_header(level, __FILE__, __LINE__)) {                              \
      printf(FMT, ##__VA_ARGS__);                                              \
      printf("%s", NORMAL);                                                    \
      fflush(stdout);                                                          \
    }                                                                          \
    log_lock(0);                                                               \
  } while (0)


#if LOGLEVEL <= VVVERBOSE_LEVEL
#define VVVERBOSE(FMT, ...) LOGGING(VVVERBOSE_LEVEL, FMT, ##__VA_ARGS__)
#else
#define VVVERBOSE(FMT, ...)
#endif

#if LOGLEVEL <= VVERBOSE_LEVEL
#define VVERBOSE(FMT, ...) LOGGING(VVERBOSE_LEVEL, FMT, ##__VA_ARGS__)
#else
#define VVERBOSE(FMT, ...)
#endif

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

static inline void log_lock(int lock) {
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

static inline int _log_header(int level, const char *file, int line) {
  if (level < LOGLEVEL) {
    return 0;
  }

  switch (level) {
  case VVVERBOSE_LEVEL:
    printf("%s[VVV]     ", CYAN);
    break;
  case VVERBOSE_LEVEL:
    printf("%s[VV]      ", CYAN);
    break;
  case VERBOSE_LEVEL:
    printf("%s[VERBOSE] ", MAGENTA);
    break;
  case DEBUG_LEVEL:
    printf("%s[DEBUG]   ", BLUE);
    break;
  case INFO_LEVEL:
    printf("%s[INFO]    ", GREEN);
    break;
  case WARNING_LEVEL:
    printf("%s[WARNING] ", YELLOW);
    break;
  case SEVERE_LEVEL:
    printf("%s[ERROR]   ", RED);
    break;
  default:
    printf("in logging should not be here\n");
    break;
  }

  char buffer[30];
  struct timeval tv;
  time_t curtime;

  gettimeofday(&tv, NULL);
  curtime = tv.tv_sec;
  strftime(buffer, 30, "%m-%d-%Y %T", localtime(&curtime));

  printf("%s %16s:%-4d ", buffer, strrchr(file, '/') + 1, line);
  printf("(tid=%zu): ", (unsigned long)pthread_self());

  return 1;
}

static inline void print_stack_trace(void) {

  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "stack trace: \n");
  backtrace_symbols_fd(array, size, STDERR_FILENO);
  exit(1);
}

#endif
