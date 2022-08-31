#pragma once

//
// Created by Juncheng Yang on 6/19/20.
//

#ifdef __cplusplus
extern "C" {
#endif

static inline void print_progress(double perc) {
  static double last_perc = 0;
  static time_t last_print_time = 0;
  time_t cur_time = time(NULL);

  if (perc - last_perc < 0.01 || cur_time - last_print_time < 60) {
    last_perc = perc;
    last_print_time = cur_time;
    sleep(2);
    return;
  }
  if (last_perc != 0) fprintf(stderr, "\033[A\033[2K\r");
  fprintf(stderr, "%.2f%%\n", perc);
  last_perc = perc;
  last_print_time = cur_time;
}

#ifdef __cplusplus
}
#endif
