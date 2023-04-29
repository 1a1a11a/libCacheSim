
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../utils/include/utilsMath.h"
#include "../../traceReader/reader.h"
#include "../../include/request.h"
#include "fast23.h"

using namespace std;
using namespace fast23;

void compareGroups::cal_group_metric_over_time(string trace_path, string ofilepath,
                                               int group_size) {
    vector<fast23::obj_reuse_stat> obj_reuse_stat =
        objectInfo::compute_obj_reuse_stat(trace_path);
    int n_obj = obj_reuse_stat.size();
    int idx_start = n_obj / 5 + 0;
    int idx_end = n_obj * 4 / 5;
    int pos = idx_start;

    vector<double> n_reuse_group;
    vector<double> reuse_time_group;
    n_reuse_group.reserve(group_size);
    reuse_time_group.reserve(group_size);

    vector<float> group_reuse_time_vec;
    group_reuse_time_vec.reserve((idx_end - idx_start) / group_size);

    while (pos + group_size < idx_end) {
        for (int j = 0; j < group_size; j++) {
            reuse_time_group.push_back(obj_reuse_stat.at(pos++).mean_reuse_rtime);
        }
        group_reuse_time_vec.push_back(static_cast<float>(mathUtils::mean(reuse_time_group, 0, 0)));
        reuse_time_group.clear();
    }

    ofstream ofs(ofilepath, ofstream::out | ofstream::app);
    ofs << trace_path << ":" << group_size << ":";

    for (auto &it : group_reuse_time_vec) {
        ofs << it << ",";
    }
    ofs << endl;
}


void compareGroups::cal_group_metric_utility_over_time(string trace_path, string ofilepath,
                                               int group_size) {
    reader_t *reader =
        open_trace(trace_path.c_str(), ORACLE_GENERAL_BIN, OBJ_ID_NUM, nullptr);
    request_t *req = new_request();
    int64_t n_skip = 0;
    int64_t n_req = n_skip;

    skip_n_req(reader, n_skip);
    read_one_req(reader, req);

    vector<double> utility_vec;
    double curr_utility = 0.0;
    int n_req_in_group = 0;
    while (req->valid) {
        if (req->next_access_vtime != -1 && req->next_access_vtime != INT64_MAX) {
            curr_utility += 1.0e6 / req->obj_size / (req->next_access_vtime - n_req);
        }

        n_req_in_group += 1;
        n_req += 1;
        if (n_req_in_group == group_size) {
            utility_vec.push_back(curr_utility);
            curr_utility = 0.0;
            n_req_in_group = 0;
        }

        read_one_req(reader, req);
    }

    ofstream ofs(ofilepath, ofstream::out | ofstream::app);
    ofs << trace_path << ":" << group_size << ":";

    for (auto &it : utility_vec) {
        ofs << it << ",";
    }
    ofs << endl;
}
