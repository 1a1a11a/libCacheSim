//
// Created by Juncheng Yang on 10/16/20.
//

#pragma once

#include <ctime>
#include <iostream>
#include <unistd.h>
#include <iomanip>

namespace utilsPrint {
static inline void print_time() {
    static time_t t;
    t = time(nullptr);
    struct tm *p = localtime(&t);
    std::cout << "[" << std::setw(2) << p->tm_hour << ":" << std::setw(2) << p->tm_min << ":" << std::setw(2) << p->tm_sec << "] ";
}

static inline void print_progress(double perc) {
    static double last_perc = 0;
    static time_t last_print_time = 0;
    time_t cur_time = time(nullptr);

    if (perc - last_perc < 0.01 || cur_time - last_print_time < 60) {
        last_perc = perc;
        last_print_time = cur_time;
        sleep(2);
        return;
    }
    if (last_perc != 0)
        fprintf(stdout, "\033[A\033[2K\r");
    fprintf(stdout, "%.2f%%\n", perc);
    last_perc = perc;
    last_print_time = cur_time;
}
}// namespace printutils
