//
// Created by Juncheng Yang on 4/21/20.
//

#include "utils.hpp"

#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>

using namespace std;

uint64_t Utils::convert_size_str(string sz_str) {
  uint64_t sz;
  string::size_type sz_str_sz = sz_str.length();
  assert(sz_str[sz_str_sz - 1] == 'B');
  if (sz_str[sz_str_sz - 2] == 'T') {
    sz = stol(sz_str.substr(0, sz_str_sz - 2)) * TB;
  } else if (sz_str[sz_str_sz - 2] == 'G') {
    sz = stol(sz_str.substr(0, sz_str_sz - 2)) * GB;
  } else if (sz_str[sz_str_sz - 2] == 'M') {
    sz = stol(sz_str.substr(0, sz_str_sz - 2)) * MB;
  } else if (sz_str[sz_str_sz - 2] == 'K') {
    sz = stol(sz_str.substr(0, sz_str_sz - 2)) * KB;
  } else {
    sz = stol(sz_str.substr(0, sz_str_sz - 1));
  }
  return sz;
}

uint64_t TraceUtils::merge_l1_trace(vector<string>& l1_trace_path_vec,
                                    string& l2_output_trace_path) {
  uint64_t n_req = 0;
  vector<FILE*> l1_ifile_vec;
  for (auto& l1_path : l1_trace_path_vec) {
    l1_ifile_vec.emplace_back(fopen(l1_path.c_str(), "r"));
  }
  ofstream l2_ofs(l2_output_trace_path,
                  ofstream::out | ofstream::trunc | ofstream::binary);

  auto comparer = [](const req_t& a, const req_t& b) { return a.ts > b.ts; };

  priority_queue<req_t, vector<req_t>, decltype(comparer)> pq(comparer);
  req_t req;
  uint32_t n_finished = 0;
  int n_read_field;

  for (int i = 0; i < l1_ifile_vec.size(); i++) {
    fscanf(l1_ifile_vec.at(i), "%u,%u,%u", &req.ts, &req.obj_id, &req.sz);
    req.trace_id = i;
    pq.push(req);
    //    std::cout << i << ":" << req.ts << ", " << req.obj_id << ", " <<
    //    req.sz << std::endl;
  }

  req = pq.top();
  //  std::cout << "top " << req.ts << ", " << req.obj_id << ", " << req.sz <<
  //  std::endl;

  while (n_finished < l1_trace_path_vec.size() && !pq.empty()) {
    req = pq.top();
    pq.pop();
    n_req += 1;
    l2_ofs.write((char*)(&req), 12);
    //    std::cout << "write " << req.obj_id << "\n";
    if (feof(l1_ifile_vec.at(req.trace_id))) {
      n_finished += 1;
    } else {
      n_read_field = fscanf(l1_ifile_vec.at(req.trace_id), "%u,%u,%u", &req.ts,
                            &req.obj_id, &req.sz);
      if (n_read_field == 3) {
        pq.push(req);
      } else {
        n_finished += 1;
      }
    }
  }

  for (auto& ifile : l1_ifile_vec) {
    fclose(ifile);
  }
  l2_ofs.close();
  return n_req;
}
