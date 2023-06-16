

#include <argp.h>
#include <stdbool.h>
#include <string.h>

#include <cassert>
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "../../include/libCacheSim/const.h"
#include "../../utils/include/mystr.h"
#include "../../utils/include/mysys.h"
#include "../cli_reader_utils.h"
#include "internal.h"

using namespace std;

void cal_one_hit(reader_t *reader, char *ofilepath) {
  unordered_map<uint64_t, uint32_t> freq_map;
  vector<double> one_hit_ratio_vec;

  request_t *req = new_request();

  read_one_req(reader, req);
  size_t last_size = 0;
  uint64_t n_one_hit = 0;

  while (req->valid) {
    if (freq_map.find(req->obj_id) == freq_map.end()) {
      freq_map[req->obj_id] = 1;
      n_one_hit++;
    } else {
      freq_map[req->obj_id] += 1;
      if (freq_map[req->obj_id] == 2) {
        n_one_hit--;
      }
    }

    if (freq_map.size() % 100 == 0 && freq_map.size() != last_size) {
      one_hit_ratio_vec.push_back((double)n_one_hit / freq_map.size());
      last_size = freq_map.size();

      if (one_hit_ratio_vec.size() != freq_map.size() / 100) {
        ERROR("Error: one_hit_ratio_vec.size() != freq_map.size() / 100\n");
      }
    }

    read_one_req(reader, req);
  }

  ofstream ofile(ofilepath, ios::out);
  ofile << reader->trace_path << ":";
  for (auto ratio : one_hit_ratio_vec) {
    ofile << ratio << ",";
  }
  ofile << endl;

  ofile.close();

  printf("%s in total %ld elem %ld objects %.4lf %.4lf\n", reader->trace_path,
         one_hit_ratio_vec.size(), freq_map.size(), one_hit_ratio_vec[0],
         one_hit_ratio_vec[one_hit_ratio_vec.size() - 1]);
}