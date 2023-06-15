
#include "utilsSys.h"
#include "logging.h"
#include <thread>
#include <iostream>
#include <fstream>
#include <cstdio>
#include <cstdlib>
#include <string>


#ifdef __linux__
#include <sys/sysinfo.h>
#endif

int utilsSys::set_thread_affinity(pthread_t tid) {
#ifdef __linux__
    static int last_core_id = -1;
    int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

    last_core_id++;
    last_core_id %= num_cores;
    DEBUG("assign thread affinity %d/%d\n", last_core_id, num_cores);

    cpu_set_t cpuset;
    CPU_ZERO(&cpuset);
    CPU_SET(last_core_id, &cpuset);

    int rc = pthread_setaffinity_np(tid, sizeof(cpu_set_t), &cpuset);
    if (rc != 0) {
        WARN("Error calling pthread_setaffinity_np: %d\n", rc);
    }
#endif
    return 0;
}

int utilsSys::get_n_cores() {
#ifdef __linux__

    INFO("This system has %d processors configured and "
         "%d processors available.\n",
         get_nprocs_conf(), get_nprocs());

    return get_nprocs();
#else
//    WARN("non linux system, use 4 threads as default\n");
    return static_cast<int>(std::thread::hardware_concurrency());
#endif
}

int utilsSys::get_n_available_cores() {
  int n_thread = static_cast<int>(std::thread::hardware_concurrency());
  /* read from /proc/loadavg */
  std::ifstream ifs("/proc/loadavg");
  std::string load;
  /* 1 min load */
  ifs >> load;
  /* 5 min load */
  ifs >> load;
  auto load_5min = static_cast<int>(std::stod(load));
  if (load_5min > n_thread)
    return 0;
  else
    return n_thread - load_5min;
}

void utilsSys::print_cwd() {
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        printf("Current working dir: %s\n", cwd);
    } else {
        perror("getcwd() error");
    }
}


#ifdef HAS_GLIB
void print_glib_ver(void) {
    printf("glib version %d.%d.%d %d\n", glib_major_version, glib_minor_version,
           glib_micro_version, glib_binary_age);
}
#endif
