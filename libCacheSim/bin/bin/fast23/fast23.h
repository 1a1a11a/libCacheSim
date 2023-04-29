#pragma once

#include <cinttypes>
#include <string>
#include <vector>

namespace fast23 {
struct obj_reuse_stat {
    int64_t first_access_vtime;
    int32_t n_reuse;
    double mean_reuse_rtime;
};

struct request_info {
    int64_t next_access_dist;
    int32_t obj_size;
};

struct group_reuse_stat {
    double mean_rt_mean;
    double mean_rt_std;
    double n_reuse_mean;
    double n_reuse_std;
    double utility_mean;
    double utility_std;

    group_reuse_stat(double v1, double v2, double v3, double v4)
        : mean_rt_mean(v1), mean_rt_std(v2), n_reuse_mean(v3), n_reuse_std(v4) {}
};

}// namespace fast23

namespace compareGrouping {
void cal_group_metric(std::string trace_path, std::string ofilepath,
                      std::vector<int> &group_sizes, int n_repeat,
                      double drop_frac, int n_thread);
void cal_group_metric_utility(std::string trace_path, std::string ofilepath,
                              std::vector<int> &group_sizes, int n_repeat,
                              int n_thread);
}// namespace compareGrouping

namespace objectInfo {
std::vector<fast23::obj_reuse_stat> compute_obj_reuse_stat(std::string trace_path);

int64_t convert_trace_to_requests(std::string trace_path, fast23::request_info *request_info, int64_t n_req, int64_t n_skip);
}// namespace objectInfo

namespace compareGroups {
void cal_group_metric_over_time(std::string trace_path, std::string ofilepath,
                                int group_size);
void cal_group_metric_utility_over_time(std::string trace_path, std::string ofilepath,
                                        int group_size);
}