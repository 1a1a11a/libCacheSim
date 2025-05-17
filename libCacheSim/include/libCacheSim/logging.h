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

#define LOGGING(level, ...)                \
  do {                                     \
    pthread_mutex_lock(&log_mtx);          \
    log_header(level, __FILE__, __LINE__); \
    fprintf(stderr __VA_OPT__(,) __VA_ARGS__);          \
    fprintf(stderr, "%s", NORMAL);         \
    fflush(stderr);                        \
    pthread_mutex_unlock(&log_mtx);        \
  } while (0)

#if LOGLEVEL <= VVVERBOSE_LEVEL
#define VVVERBOSE(...) LOGGING(VVVERBOSE_LEVEL, __VA_ARGS__)
#else
#define VVVERBOSE(...) \
  do {                 \
  } while (0)
#endif

#if LOGLEVEL <= VVERBOSE_LEVEL
#define VVERBOSE(...) LOGGING(VVERBOSE_LEVEL, __VA_ARGS__)
#else
#define VVERBOSE(...) \
  do {                \
  } while (0)
#endif

#if LOGLEVEL <= VERBOSE_LEVEL
#define VERBOSE(...) LOGGING(VERBOSE_LEVEL, __VA_ARGS__)
#else
#define VERBOSE(...) \
  do {               \
  } while (0)
#endif

#if LOGLEVEL <= DEBUG_LEVEL
#define DEBUG(...) LOGGING(DEBUG_LEVEL, __VA_ARGS__)
#else
#define DEBUG(...) \
  do {             \
  } while (0)
#endif

#if LOGLEVEL <= INFO_LEVEL
#define INFO(...) LOGGING(INFO_LEVEL, __VA_ARGS__)
#else
#define INFO(...) \
  do {            \
  } while (0)
#endif

#if LOGLEVEL <= WARN_LEVEL
#define WARN(...) LOGGING(WARN_LEVEL, __VA_ARGS__)
#else
#define WARN(...) \
  do {            \
  } while (0)
#endif

#if LOGLEVEL <= SEVERE_LEVEL
#define ERROR(...)                      \
  {                                     \
    LOGGING(SEVERE_LEVEL, __VA_ARGS__); \
    abort();                            \
  }
#else
#define ERROR(...)
#endif

#define WARN_ONCE(...)           \
  do {                           \
    static bool printed = false; \
    if (!printed) {              \
      WARN(__VA_ARGS__);         \
      printed = true;            \
      fflush(stdout);            \
    }                            \
  } while (0)

#define DEBUG_ONCE(...)          \
  do {                           \
    static bool printed = false; \
    if (!printed) {              \
      DEBUG(__VA_ARGS__);        \
      printed = true;            \
      fflush(stdout);            \
    }                            \
  } while (0)

#define INFO_ONCE(...)           \
  do {                           \
    static bool printed = false; \
    if (!printed) {              \
      WARN(__VA_ARGS__);         \
      printed = true;            \
      fflush(stdout);            \
    }                            \
  } while (0)

static inline void log_header(int level, const char *file, int line) {
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
  time_t curtime = time(NULL);

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
