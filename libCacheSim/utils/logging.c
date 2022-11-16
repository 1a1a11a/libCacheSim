//
// Created by Juncheng on 6/1/21.
//

#include <execinfo.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

pthread_mutex_t log_mtx = PTHREAD_MUTEX_INITIALIZER;

void print_stack_trace(void) {
  void *array[10];
  size_t size;

  // get void*'s for all entries on the stack
  size = backtrace(array, 10);

  // print out all the frames to stderr
  fprintf(stderr, "stack trace: \n");
  backtrace_symbols_fd(array, size, STDERR_FILENO);
}
