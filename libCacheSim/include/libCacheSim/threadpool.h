// Adapted from: https://nachtimwald.com/2019/04/12/thread-pool-in-c/
// Originally Copyright (c) 2019 John Schember
// License: MIT License

#ifndef INCLUDE_LIBCACHESIM_THREADPOOL_H
#define INCLUDE_LIBCACHESIM_THREADPOOL_H

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

typedef void (*func_t)(void *arg1, void *arg2);

struct threadpool_job {
  func_t func;
  void *arg1;
  void *arg2;
  struct threadpool_job *next;
};
// linked list of waiting threads
// similar to GThreadPool we make there 2 arguments
typedef struct threadpool_job threadpool_job_t;

struct threadpool {
  threadpool_job_t *job_first;
  threadpool_job_t *job_last;
  pthread_mutex_t job_mutex;
  pthread_cond_t job_cond;
  pthread_cond_t working_cond;
  size_t working_cnt;
  size_t thread_cnt;
  bool stop;
};
typedef struct threadpool threadpool_t;

bool threadpool_create(threadpool_t *tm, size_t num);
void threadpool_destroy(threadpool_t *tm);

bool threadpool_push(threadpool_t *tm, func_t func, void *arg, void *arg2);
void threadpool_wait(threadpool_t *tm);

#endif