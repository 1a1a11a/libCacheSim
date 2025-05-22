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

/**
 * @brief Create a fixed size thread pool on heap.
 * @param tm Pointer to the thread pool structure
 * @param num Number of threads in the pool
 * @note `tm` MUST be allocated on the *heap* before this call
 */
bool threadpool_create(threadpool_t *tm, size_t num);

/**
 * @brief Destroy the thread pool and free its associated memory.
 * @param tm Pointer to the thread pool structure
 * @note The memory is also freed, so `tm` MUST NOT be reused after this call.
 */
void threadpool_destroy(threadpool_t *tm);


/**
 * @brief Push a job to the thread pool. There is no limit on the number of waiting jobs.
 * @param tm Pointer to the thread pool structure
 * @param func Function to be executed in thread pool
 * @param arg1 First argument to `func`
 * @param arg2 Second argument to `func`
 * @note `func` MUST take 2 pointer arguments only. Prepare arguments in a struct if needed.
 */
bool threadpool_push(threadpool_t *tm, func_t func, void *arg, void *arg2);

/**
 * @brief Wait for all jobs to finish. Usually there is no need to manually call this function.
 * @param tm Pointer to the thread pool structure
 */
void threadpool_wait(threadpool_t *tm);

#endif