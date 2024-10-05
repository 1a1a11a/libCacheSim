#ifndef LOGGING_H
#define LOGGING_H

#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "../config.h"
#include "const.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void log_header(int level, const char *file, int line);
void print_stack_trace(void);
extern pthread_mutex_t log_mtx;

#define LOGGING(level, FMT, ...)           \
  do {                                     \
    pthread_mutex_lock(&log_mtx);          \
    log_header(level, __FILE__, __LINE__); \
    fprintf(stderr, FMT, ##__VA_ARGS__);            \
    fprintf(stderr, "%s", NORMAL);                  \
    fflush(stderr);                        \
    pthread_mutex_unlock(&log_mtx);        \
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

#if LOGLEVEL <= WARN_LEVEL
#define WARN(FMT, ...) LOGGING(WARN_LEVEL, FMT, ##__VA_ARGS__)
#else
#define WARN(FMT, ...)
#endif

#if LOGLEVEL <= SEVERE_LEVEL
#define ERROR(FMT, ...)                        \
  {                                            \
    LOGGING(SEVERE_LEVEL, FMT, ##__VA_ARGS__); \
    abort();                                   \
  }
#else
#define ERROR(FMT, ...)
#endif

#define WARN_ONCE(FMT, ...)      \
  do {                           \
    static bool printed = false; \
    if (!printed) {              \
      WARN(FMT, ##__VA_ARGS__);  \
      printed = true;            \
      fflush(stdout);            \
    }                            \
  } while (0)

#define DEBUG_ONCE(FMT, ...)     \
  do {                           \
    static bool printed = false; \
    if (!printed) {              \
      WARN(FMT, ##__VA_ARGS__);  \
      printed = true;            \
      fflush(stdout);            \
    }                            \
  } while (0)

#define INFO_ONCE(FMT, ...)      \
  do {                           \
    static bool printed = false; \
    if (!printed) {              \
      WARN(FMT, ##__VA_ARGS__);  \
      printed = true;            \
      fflush(stdout);            \
    }                            \
  } while (0)

static inline void log_header(int level, const char *file, int line) {
  //  if (level < LOGLEVEL) {
  //    return 0;
  //  }

  switch (level) {
    case VVVERBOSE_LEVEL:
      fprintf(stderr, "%s[VVV]   ", CYAN);
      break;
    case VVERBOSE_LEVEL:
      fprintf(stderr, "%s[VV]    ", CYAN);
      break;
    case VERBOSE_LEVEL:
      fprintf(stderr, "%s[VERB]  ", MAGENTA);
      break;
    case DEBUG_LEVEL:
      fprintf(stderr, "%s[DEBUG] ", CYAN);
      break;
    case INFO_LEVEL:
      fprintf(stderr, "%s[INFO]  ", GREEN);
      break;
    case WARN_LEVEL:
      fprintf(stderr, "%s[WARN]  ", YELLOW);
      break;
    case SEVERE_LEVEL:
      fprintf(stderr, "%s[ERROR] ", RED);
      break;
    default:
      fprintf(stderr, "in logging should not be here\n");
      break;
  }

  char buffer[30];
  struct timeval tv;
  time_t curtime;

  gettimeofday(&tv, NULL);
  curtime = tv.tv_sec;
  strftime(buffer, 30, "%m-%d-%Y %T", localtime(&curtime));

  fprintf(stderr, "%s %8s:%-4d ", buffer, strrchr(file, '/') + 1, line);
  fprintf(stderr, "(tid=%zu): ", (unsigned long)pthread_self());
}

#ifdef __cplusplus
}
#endif

#endif
