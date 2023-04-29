//
// Created by Juncheng Yang on 10/16/20.
//

#pragma once

#include <pthread.h>

namespace utilsSys {

int set_thread_affinity(pthread_t tid);
int get_n_cores();
int get_n_available_cores();
void print_cwd();


}// namespace utilsSys
