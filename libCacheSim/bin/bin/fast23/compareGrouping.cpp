
#include <boost/asio/post.hpp>
#include <boost/asio/thread_pool.hpp>
#include <cinttypes>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <unistd.h>
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../traceReader/reader.h"
#include "../../include/request.h"
#include "../../utils/include/utilsMath.h"
#include "fast23.h"

using namespace std;
using namespace fast23;

/**
 * @brief calculate the mean and std of 1. reuse time, 2. n_reuse of a
 * write-time-based object group and a random object group, repeat this n_repeat
 * times.
 * The result is written to ofile
 *
 * @param obj_reuse_stat
 * @param idx_start
 * @param idx_end
 * @param group_size
 * @param n_repeat
 * @param drop_frac drop the drop_frac of the smallest reuse_time and largest
 * n_reuse in the group
 * @param ofilepath
 * @param mtx
 */
void cal_group_metric_task(vector<fast23::obj_reuse_stat> &obj_reuse_stat,
                           int idx_start, int idx_end, int group_size,
                           int n_repeat, double drop_frac, string ofilepath,
                           mutex *mtx) {
    if (idx_start + group_size + n_repeat >= idx_end) return;

    std::random_device dev;
    std::mt19937 rng(dev());
    uniform_int_distribution<mt19937::result_type> dist(idx_start,
                                                        idx_end - group_size);

    vector<fast23::group_reuse_stat> seq_group_reuse_stat_vec;
    vector<fast23::group_reuse_stat> rnd_group_reuse_stat_vec;

    /** instead of generating group_size random indexes,
   * which may have duplicate, we generate a shuffle index vector,
   * and then use the shuffle index vector to generate group_size random indexes
   **/
    vector<int> rnd_idx_vec;
    rnd_idx_vec.reserve(idx_end - idx_start);
    for (int i = idx_start; i < idx_end; i++) {
        rnd_idx_vec.push_back(i);
    }
    int rnd_idx_vec_idx = rnd_idx_vec.size();

    vector<double> n_reuse_group;
    vector<double> reuse_time_group;
    n_reuse_group.reserve(group_size);
    reuse_time_group.reserve(group_size);

    for (int i = 0; i < n_repeat; i++) {
        // compute for sequential group
        int seq_base_idx = dist(rng);
        for (int j = 0; j < group_size; j++) {
            int idx = seq_base_idx + j;
            n_reuse_group.push_back((double) obj_reuse_stat.at(idx).n_reuse);
            if (obj_reuse_stat.at(idx).n_reuse > 0) {
                reuse_time_group.push_back(obj_reuse_stat.at(idx).mean_reuse_rtime);
            }
        }

        int n_drop_n_reuse = (int) (drop_frac * n_reuse_group.size());
        int n_drop_reuse_time = (int) (drop_frac * reuse_time_group.size());
        if (n_drop_n_reuse > 0) {
            sort(n_reuse_group.begin(), n_reuse_group.end(), greater<double>());
            sort(reuse_time_group.begin(), reuse_time_group.end());
        }
        seq_group_reuse_stat_vec.emplace_back(
            mathUtils::mean(reuse_time_group, n_drop_reuse_time, 0),
            mathUtils::stdev(reuse_time_group, n_drop_reuse_time, 0),
            mathUtils::mean(n_reuse_group, n_drop_n_reuse, 0),
            mathUtils::stdev(n_reuse_group, n_drop_n_reuse, 0));
        n_reuse_group.clear();
        reuse_time_group.clear();

        // compute for random group
        if (rnd_idx_vec_idx + group_size >= rnd_idx_vec.size()) {
            rnd_idx_vec_idx = 0;
            shuffle(rnd_idx_vec.begin(), rnd_idx_vec.end(), rng);
        }
        for (int j = 0; j < group_size; j++) {
            int idx = rnd_idx_vec.at(rnd_idx_vec_idx + j);
            n_reuse_group.push_back((double) obj_reuse_stat.at(idx).n_reuse);
            if (obj_reuse_stat.at(idx).mean_reuse_rtime != 0) {
                reuse_time_group.push_back(obj_reuse_stat.at(idx).mean_reuse_rtime);
            }
        }
        rnd_idx_vec_idx += group_size;

        n_drop_n_reuse = (int) (drop_frac * n_reuse_group.size());
        n_drop_reuse_time = (int) (drop_frac * reuse_time_group.size());
        if (n_drop_n_reuse > 0) {
            sort(n_reuse_group.begin(), n_reuse_group.end(), greater<double>());
            sort(reuse_time_group.begin(), reuse_time_group.end());
        }
        rnd_group_reuse_stat_vec.emplace_back(
            mathUtils::mean(reuse_time_group, n_drop_reuse_time, 0),
            mathUtils::stdev(reuse_time_group, n_drop_reuse_time, 0),
            mathUtils::mean(n_reuse_group, n_drop_n_reuse, 0),
            mathUtils::stdev(n_reuse_group, n_drop_n_reuse, 0));
        n_reuse_group.clear();
        reuse_time_group.clear();
    }

    auto guard = std::lock_guard(*mtx);
    string seq_ofilepath = ofilepath + ".grp" + to_string(group_size) + ".seq";
    string rnd_ofilepath = ofilepath + ".grp" + to_string(group_size) + ".rnd";
    ofstream seq_ofs(seq_ofilepath, ofstream::out | ofstream::app);
    ofstream rnd_ofs(rnd_ofilepath, ofstream::out | ofstream::app);

    for (auto &it : seq_group_reuse_stat_vec) {
        assert(it.mean_rt_mean >= 0);
        seq_ofs << it.mean_rt_mean << "," << it.mean_rt_std << ","
                << it.n_reuse_mean << "," << it.n_reuse_std << "\n";
    }
    for (auto &it : rnd_group_reuse_stat_vec) {
        assert(it.mean_rt_mean >= 0);
        rnd_ofs << it.mean_rt_mean << "," << it.mean_rt_std << ","
                << it.n_reuse_mean << "," << it.n_reuse_std << "\n";
    }

    seq_ofs.close();
    rnd_ofs.close();
}

/**
 * @brief calculate the mean and std of 1. reuse time, 2. n_reuse of a
 * write-time-based object group and a random object group for different group sizes, 
 * 
 * repeat this n_repeat times, and drop the top drop_frac popular objects
 * note that drop_frac is not used and should be 0.0
 * 
 * 
 * @param trace_path 
 * @param ofilepath 
 * @param group_sizes 
 * @param n_repeat 
 * @param drop_frac 
 * @param n_thread 
 */
void compareGrouping::cal_group_metric(string trace_path, string ofilepath,
                                       vector<int> &group_sizes, int n_repeat,
                                       double drop_frac, int n_thread) {
    vector<fast23::obj_reuse_stat> obj_reuse_stat =
        objectInfo::compute_obj_reuse_stat(trace_path);
    int n_obj = obj_reuse_stat.size();
    /* drop the first and last 20% of objects */
    int idx_start = n_obj / 5;
    int idx_end = n_obj * 4 / 5;

    std::mutex mtx;
    boost::asio::thread_pool workers(n_thread);

    for (auto group_size : group_sizes) {
        int batch_size = std::max(100000 / group_size, 20);
        int n_batch = (n_repeat + batch_size - 1) / batch_size;
        cout << "group_size: " << group_size << " batch_size: " << batch_size
             << " n_batch: " << n_batch << endl;

        if (n_thread == 1) {
            cal_group_metric_task(obj_reuse_stat, idx_start, idx_end, group_size,
                                  n_repeat, drop_frac, ofilepath, &mtx);
        } else {
            for (int i = 0; i < n_batch; i++) {
                boost::asio::post(workers, [&, group_size, batch_size] {
                    cal_group_metric_task(obj_reuse_stat, idx_start, idx_end,
                                          group_size, batch_size, drop_frac, ofilepath,
                                          &mtx);
                });
            }
        }
    }

    workers.join();
    return;
}

void cal_group_metric_utility_seq_task(struct request_info *request_info, size_t request_info_size, int group_size, int n_repeat, string ofilepath, mutex *mtx) {
    std::random_device dev;
    std::mt19937 rng(dev());
    uniform_int_distribution<mt19937::result_type> dist(0, request_info_size - group_size);

    vector<double> utility_mean_vec;
    vector<double> utility_std_vec;
    utility_mean_vec.reserve(n_repeat);
    utility_std_vec.reserve(n_repeat);

    for (int i = 0; i < n_repeat; i++) {
        vector<double> utility_vec;
        size_t start_idx = dist(rng);
        // size_t start_idx = 0;
        for (int j = 0; j < group_size; j++) {
            int idx = start_idx + j;
            if (request_info[idx].next_access_dist == -1) {
                utility_vec.push_back(0);
            } else {
                utility_vec.push_back(1000000.0 / ((double) request_info[idx].next_access_dist * (double) request_info[idx].obj_size));
            }
        }
        // std::sort(utility_vec.begin(), utility_vec.end());
        // printf("%.4lf %.4lf %.4lf %.4lf, %.4lf %.4lf %.4lf %.4lf\n", utility_vec[0], utility_vec[1], utility_vec[2], utility_vec[3],
        // utility_vec[group_size - 4], utility_vec[group_size - 3], utility_vec[group_size - 2], utility_vec[group_size - 1]);
        utility_mean_vec.push_back(mathUtils::mean(utility_vec, 0, 0));
        utility_std_vec.push_back(mathUtils::stdev(utility_vec, 0, 0));
    }

    auto guard = std::lock_guard(*mtx);
    ofstream ofs(ofilepath + ".grp" + to_string(group_size) + ".seq", ofstream::out | ofstream::app);

    for (int i = 0; i < n_repeat; i++) {
        ofs << utility_mean_vec.at(i) << "," << utility_std_vec.at(i) << "\n";
    }

    ofs.close();
}

void cal_group_metric_utility_rnd_task(struct request_info *request_info, size_t request_info_size, int group_size, int n_repeat, string ofilepath, mutex *mtx) {
    std::random_device dev;
    std::mt19937 rng(dev());
    uniform_int_distribution<mt19937::result_type> dist(0, request_info_size);

    vector<size_t> rnd_idx_vec;
    rnd_idx_vec.resize(request_info_size);
    for (int i = 0; i < request_info_size; i++) {
        rnd_idx_vec[i] = i;
    }
    int rnd_idx_vec_pos = request_info_size;
    vector<double> utility_mean_vec;
    vector<double> utility_std_vec;
    utility_mean_vec.reserve(n_repeat);
    utility_std_vec.reserve(n_repeat);

    for (int i = 0; i < n_repeat; i++) {
        if (rnd_idx_vec_pos + group_size >= request_info_size) {
            rnd_idx_vec_pos = 0;
            shuffle(rnd_idx_vec.begin(), rnd_idx_vec.end(), rng);
        }
        vector<double> utility_vec;
        for (int j = 0; j < group_size; j++) {
            int idx = rnd_idx_vec.at(rnd_idx_vec_pos + j);
            if ((double) request_info[idx].next_access_dist == -1) {
                utility_vec.push_back(0);
            } else {
                utility_vec.push_back(1000000.0 / ((double) request_info[idx].next_access_dist * (double) request_info[idx].obj_size));
            }
        }
        rnd_idx_vec_pos += group_size;
        // std::sort(utility_vec.begin(), utility_vec.end());
        utility_mean_vec.push_back(mathUtils::mean(utility_vec, 0, 0));
        utility_std_vec.push_back(mathUtils::stdev(utility_vec, 0, 0));
    }

    auto guard = std::lock_guard(*mtx);
    ofstream ofs(ofilepath + ".grp" + to_string(group_size) + ".rnd", ofstream::out | ofstream::app);

    for (int i = 0; i < n_repeat; i++) {
        ofs << utility_mean_vec.at(i) << "," << utility_std_vec.at(i) << "\n";
    }

    ofs.close();
}

/**
 * @brief calculate the mean and std of 1. reuse time, 2. n_reuse of a
 * write-time-based object group and a random object group for different group sizes, 
 * 
 * repeat this n_repeat times, and drop the top drop_frac popular objects
 * note that drop_frac is not used and should be 0.0
 * 
 * 
 * @param trace_path 
 * @param ofilepath 
 * @param group_sizes 
 * @param n_repeat 
 * @param drop_frac 
 * @param n_thread 
 */
void compareGrouping::cal_group_metric_utility(string trace_path, string ofilepath,
                                               vector<int> &group_sizes, int n_repeat,
                                               int n_thread) {
    reader_t *reader =
        open_trace(trace_path.c_str(), ORACLE_GENERAL_BIN, OBJ_ID_NUM, nullptr);
    request_t *req = new_request();
    int64_t n_total_req = get_num_of_req(reader);
    int64_t n_skip = n_total_req / 10;
    int64_t n_req = n_skip;
    fast23::request_info *request_info = new fast23::request_info[n_total_req - n_skip];

    skip_n_req(reader, n_skip);
    read_one_req(reader, req);

    while (req->valid) {
        if (req->next_access_vtime != -1 && req->next_access_vtime != INT64_MAX)
            request_info[n_req - n_skip].next_access_dist = req->next_access_vtime - n_req;
        else
            request_info[n_req - n_skip].next_access_dist = -1;
        request_info[n_req - n_skip].obj_size = req->obj_size;

        read_one_req(reader, req);
        n_req += 1;
    }

    std::mutex mtx;
    boost::asio::thread_pool workers(n_thread);

    for (auto group_size : group_sizes) {
        int batch_size = std::max(100000 / group_size, 100);
        // batch_size = 100000;
        int n_batch = (n_repeat + batch_size - 1) / batch_size;
        cout << "group_size: " << group_size << " batch_size: " << batch_size
             << " n_batch: " << n_batch << endl;

        if (n_thread == 1) {
            cal_group_metric_utility_seq_task(request_info, n_req, group_size,
                                              n_repeat, ofilepath, &mtx);
            cal_group_metric_utility_rnd_task(request_info, n_req, group_size,
                                              n_repeat, ofilepath, &mtx);
        } else {
            for (int i = 0; i < n_batch; i++) {
                boost::asio::post(workers, [&, group_size, batch_size, request_info] {
                    cal_group_metric_utility_seq_task(request_info, n_req, group_size, batch_size, ofilepath,
                                                      &mtx);
                });
                boost::asio::post(workers, [&, group_size, batch_size, request_info] {
                    cal_group_metric_utility_rnd_task(request_info, n_req, group_size, batch_size, ofilepath,
                                                      &mtx);
                });
            }
        }
    }

    workers.join();
    return;
}
