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

void print_progress(double perc) {
  static double last_perc = 0;
  static time_t last_print_time = 0;
  time_t cur_time = time(NULL);

  if (perc - last_perc < 0.01 || cur_time - last_print_time < 60) {
    last_perc = perc;
    last_print_time = cur_time;
    sleep(2);
    return;
  }
  if (last_perc != 0) fprintf(stdout, "\033[A\033[2K\r");
  fprintf(stdout, "%.2f%%...\n", perc);
  last_perc = perc;
  last_print_time = cur_time;
}
