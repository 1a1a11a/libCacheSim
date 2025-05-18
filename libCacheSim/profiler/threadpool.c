// Adapted from: https://nachtimwald.com/2019/04/12/thread-pool-in-c/
// Originally Copyright (c) 2019 John Schember
// License: MIT License

#include "../include/libCacheSim/threadpool.h"

#include <pthread.h>
#include <stdlib.h>

static threadpool_job_t *threadpool_job_create(func_t func, void *arg1,
                                               void *arg2) {
  threadpool_job_t *job;

  if (func == NULL) return NULL;

  job = malloc(sizeof(*job));
  job->func = func;
  job->arg1 = arg1;
  job->arg2 = arg2;
  job->next = NULL;
  return job;
}

static void threadpool_job_destroy(threadpool_job_t *job) {
  if (job == NULL) return;
  free(job);
}

static threadpool_job_t *threadpool_job_get(threadpool_t *tm) {
  threadpool_job_t *job;

  if (tm == NULL) return NULL;

  job = tm->job_first;
  if (job == NULL) return NULL;

  if (job->next == NULL) {
    tm->job_first = NULL;
    tm->job_last = NULL;
  } else {
    tm->job_first = job->next;
  }

  return job;
}

static void *threadpool_worker(void *arg) {
  threadpool_t *tm = arg;
  threadpool_job_t *job;

  while (1) {
    pthread_mutex_lock(&(tm->job_mutex));

    while (tm->job_first == NULL && !tm->stop)
      pthread_cond_wait(&(tm->job_cond), &(tm->job_mutex));

    if (tm->stop) break;

    job = threadpool_job_get(tm);
    tm->working_cnt++;
    pthread_mutex_unlock(&(tm->job_mutex));

    if (job != NULL) {
      job->func(job->arg1, job->arg2);
      threadpool_job_destroy(job);
    }

    pthread_mutex_lock(&(tm->job_mutex));
    tm->working_cnt--;
    if (!tm->stop && tm->working_cnt == 0 && tm->job_first == NULL)
      pthread_cond_signal(&(tm->working_cond));
    pthread_mutex_unlock(&(tm->job_mutex));
  }

  tm->thread_cnt--;
  pthread_cond_signal(&(tm->working_cond));
  pthread_mutex_unlock(&(tm->job_mutex));
  return NULL;
}

bool threadpool_create(threadpool_t *tm, size_t num) {
  pthread_t thread;
  size_t i;

  if (num == 0) num = 2;

  tm->thread_cnt = num;

  pthread_mutex_init(&(tm->job_mutex), NULL);
  pthread_cond_init(&(tm->job_cond), NULL);
  pthread_cond_init(&(tm->working_cond), NULL);

  tm->job_first = NULL;
  tm->job_last = NULL;

  for (i = 0; i < num; i++) {
    int res = pthread_create(&thread, NULL, threadpool_worker, tm);
    if (res != 0) {
      if (i == 0)
        return false;
      else {
        tm->thread_cnt = i;
        threadpool_destroy(tm);
        return false;
      }
    }
    pthread_detach(thread);
  }

  return true;
}

// do not call free on tm after calling this function
void threadpool_destroy(threadpool_t *tm) {
  threadpool_job_t *job;
  threadpool_job_t *job2;

  if (tm == NULL) return;

  pthread_mutex_lock(&(tm->job_mutex));
  job = tm->job_first;
  while (job != NULL) {
    job2 = job->next;
    threadpool_job_destroy(job);
    job = job2;
  }
  tm->job_first = NULL;
  tm->stop = true;
  pthread_cond_broadcast(&(tm->job_cond));
  pthread_mutex_unlock(&(tm->job_mutex));

  threadpool_wait(tm);

  pthread_mutex_destroy(&(tm->job_mutex));
  pthread_cond_destroy(&(tm->job_cond));
  pthread_cond_destroy(&(tm->working_cond));

  free(tm);
}

// similar to GThreadPool, we make there 2 arguments
// but we have them pushed to pool together
bool threadpool_push(threadpool_t *tm, func_t func, void *arg1, void *arg2) {
  threadpool_job_t *job;

  if (tm == NULL) return false;

  job = threadpool_job_create(func, arg1, arg2);
  if (job == NULL) return false;

  pthread_mutex_lock(&(tm->job_mutex));
  if (tm->job_first == NULL) {
    tm->job_first = job;
    tm->job_last = tm->job_first;
  } else {
    tm->job_last->next = job;
    tm->job_last = job;
  }

  pthread_cond_broadcast(&(tm->job_cond));
  pthread_mutex_unlock(&(tm->job_mutex));

  return true;
}

void threadpool_wait(threadpool_t *tm) {
  if (tm == NULL) return;

  pthread_mutex_lock(&(tm->job_mutex));
  while (1) {
    if (tm->job_first != NULL || (!tm->stop && tm->working_cnt != 0) ||
        (tm->stop && tm->thread_cnt != 0)) {
      pthread_cond_wait(&(tm->working_cond), &(tm->job_mutex));
    } else {
      break;
    }
  }
  pthread_mutex_unlock(&(tm->job_mutex));
}
