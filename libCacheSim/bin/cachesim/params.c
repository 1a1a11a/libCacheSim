
#include <stdbool.h>

#include "internal.h"

/******* script to generate the code ********/
/**
def get_wss(datafile="traceStatMsr"):
    with open(datafile) as ifile:
        data_dict = {}
        dataname = ""
        wss_byte, wss_obj = 0, 0
        for line in ifile:
            if line[:4] == "dat:":
                dataname = line.split("/")[-1].split(".")[0]
            elif line.startswith("number of requests"):
                wss_obj = line.split()[7]
            elif line.startswith("number of req GiB"):
                wss_byte = float(line.split()[9]) * 1024 * 1024 * 1024
                data_dict[dataname] = (wss_byte, wss_obj)
                print('''else if (strstr(trace_path, "{}") != NULL) {{
    wss_byte = {};
    wss_obj = {};
}}'''.format(dataname, wss_byte, wss_obj), end=" ")

if __name__ == "__main__":
    get_wss()


**/

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief retrieve the working set size (in bytes)
 * of the msr trace
 *
 * @param tracepath
 * @return uint64_t
 */
static uint64_t get_msr_wss(char *trace_path) {
  uint64_t wss_byte = 0;
  uint64_t wss_obj = 0;
  if (strstr(trace_path, "hm_0") != NULL) {
    wss_byte = 5627588273;
    wss_obj = 439187;
  } else if (strstr(trace_path, "proj_0") != NULL) {
    wss_byte = 11714738048;
    wss_obj = 286228;
  } else if (strstr(trace_path, "web_2") != NULL) {
    wss_byte = 71496495215;
    wss_obj = 1321270;
  } else if (strstr(trace_path, "prn_0") != NULL) {
    wss_byte = 21954476952;
    wss_obj = 711385;
  } else if (strstr(trace_path, "prxy_0") != NULL) {
    wss_byte = 2181521263;
    wss_obj = 155681;
  } else if (strstr(trace_path, "proj_4") != NULL) {
    wss_byte = 133694064112;
    wss_obj = 3002525;
  } else if (strstr(trace_path, "proj_2") != NULL) {
    wss_byte = 726042317055;
    wss_obj = 16180242;
  } else if (strstr(trace_path, "proj_1") != NULL) {
    wss_byte = 754502595719;
    wss_obj = 15452001;
  } else if (strstr(trace_path, "usr_2") != NULL) {
    wss_byte = 421399857253;
    wss_obj = 7374757;
  } else if (strstr(trace_path, "src1_1") != NULL) {
    wss_byte = 197931420352;
    wss_obj = 6170590;
  } else if (strstr(trace_path, "prn_1") != NULL) {
    wss_byte = 92634821007;
    wss_obj = 2173575;
  } else if (strstr(trace_path, "usr_1") != NULL) {
    wss_byte = 730508653546;
    wss_obj = 13966057;
  } else if (strstr(trace_path, "prxy_1") != NULL) {
    wss_byte = 16482259120;
    wss_obj = 390226;
  } else if (strstr(trace_path, "src1_0") != NULL) {
    wss_byte = 164910638039;
    wss_obj = 5659341;
  }

  return wss_byte;
}

bool set_hard_code_cache_size(struct arguments *args) {
  if (args->ignore_obj_size) {
    return false;
  }
  
  if (strstr(args->trace_path, "cphy") != NULL) {
    uint64_t s[10] = {500,   1000,  2000,  4000,  8000,
                      12000, 16000, 24000, 32000, 64000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = sizeof(s) / sizeof(uint64_t);
  } else if (strstr(args->trace_path, "msr") != NULL) {
    uint64_t wss_byte = get_msr_wss(args->trace_path);
    double s[12] = {0.0001, 0.0002, 0.0005, 0.001, 0.002, 0.005,
                    0.01,   0.02,   0.05,   0.1,   0.2,   0.5};
    int pos = 0;
    for (int i = 0; i < sizeof(s) / sizeof(double); i++) {
      if (wss_byte * s[i] > 20 * MiB) {
        args->cache_sizes[pos++] = wss_byte * s[i];
      }
    }

    args->n_cache_size = pos;
  } else if (strstr(args->trace_path, "wiki") != NULL) {
    uint64_t s[12] = {10,  20,   50,   100,  200,  400,
                      800, 1200, 1600, 2400, 3200, 6400};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = s[i] * 1024 * MiB;
    }
    args->n_cache_size = sizeof(s) / sizeof(uint64_t);
  } else if (strstr(args->trace_path, "cluster") != NULL) {
    uint64_t s[9] = {64, 128, 256, 512, 1024, 2048, 4096, 8192, 16384};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = sizeof(s) / sizeof(uint64_t);
  } else {
    uint64_t s[8] = {50, 100, 400, 1000, 2000, 4000, 8000, 16000};
    for (int i = 0; i < sizeof(s) / sizeof(uint64_t); i++) {
      args->cache_sizes[i] = MiB * s[i];
    }
    args->n_cache_size = sizeof(s) / sizeof(uint64_t);
    return false;
  }

  return true;
}

#ifdef __cplusplus
}
#endif
