/**
 * compare the write-time-based grouping and random grouping 
 * 
 */

#include <unistd.h>

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
#include <unordered_map>
#include <utility>
#include <vector>

#include "../../include/request.h"
#include "../../traceReader/reader.h"
#include "../../utils/include/utilsMath.h"
#include "../utils.h"

#include "fast23.h"

using namespace fast23;

struct map_val {
    int64_t first_acces_vtime;
    int32_t last_access_rtime;
    std::vector<double> reuse_rtime_vec;
};

std::vector<fast23::obj_reuse_stat> objectInfo::compute_obj_reuse_stat(std::string trace_path) {
    reader_t *reader =
        open_trace(trace_path.c_str(), ORACLE_GENERAL_BIN, OBJ_ID_NUM, nullptr);
    request_t *req = new_request();
    int64_t n_req = 0;
    unordered_map<uint64_t, shared_ptr<struct map_val>> objmap;

    read_one_req(reader, req);
    uint32_t start_ts = req->clock_time;
    while (req->valid) {
        n_req += 1;
        if (objmap.find(req->obj_id) == objmap.end()) {
            shared_ptr<struct map_val> vptr = make_shared<struct map_val>();
            vptr->first_acces_vtime = n_req;
            vptr->last_access_rtime = static_cast<int32_t>(req->clock_time);
            objmap.emplace(req->obj_id, std::move(vptr));
        } else {
            shared_ptr<struct map_val> &vptr = objmap[req->obj_id];
            double reuse_rtime =
                static_cast<double>(req->clock_time - vptr->last_access_rtime);
            assert(reuse_rtime >= 0);
            vptr->reuse_rtime_vec.push_back(reuse_rtime);
            vptr->last_access_rtime = static_cast<int32_t>(req->clock_time);
        }

        read_one_req(reader, req);
    }

    cout << objmap.size() << " objects, " << n_req << " requests, "
         << (req->clock_time - start_ts) / 3600 << " hr\n";

    close_trace(reader);

    vector<fast23::obj_reuse_stat> obj_reuse_stat_vec;
    for (auto &it : objmap) {
        fast23::obj_reuse_stat obj_reuse_stat = {
            it.second->first_acces_vtime,
            (int32_t) it.second->reuse_rtime_vec.size(),
            mathUtils::mean(it.second->reuse_rtime_vec, 0, 0)};
        assert(obj_reuse_stat.mean_reuse_rtime >= 0);

        obj_reuse_stat_vec.push_back(obj_reuse_stat);
    }

    std::sort(obj_reuse_stat_vec.begin(), obj_reuse_stat_vec.end(),
              [](const fast23::obj_reuse_stat &a, const fast23::obj_reuse_stat &b) {
                  return a.first_access_vtime < b.first_access_vtime;
              });

    return std::move(obj_reuse_stat_vec);
}

int64_t objectInfo::convert_trace_to_requests(std::string trace_path, fast23::request_info *request_info, int64_t request_info_size, int64_t n_skip) {
    reader_t *reader =
        open_trace(trace_path.c_str(), ORACLE_GENERAL_BIN, OBJ_ID_NUM, nullptr);
    request_t *req = new_request();
    int64_t n_req = n_skip;

    skip_n_req(reader, n_skip);
    read_one_req(reader, req);

    while (req->valid && n_req - n_skip < request_info_size) {
        if (req->next_access_vtime != -1 && req->next_access_vtime != INT64_MAX)
            request_info[n_req - n_skip].next_access_dist = req->next_access_vtime - n_req;
        else 
            request_info[n_req - n_skip].next_access_dist = -1;
        request_info[n_req - n_skip].obj_size = req->obj_size;

        read_one_req(reader, req);
        n_req += 1;
    }

    return n_req - n_skip;
}
