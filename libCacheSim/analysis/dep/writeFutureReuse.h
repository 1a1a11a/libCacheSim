
#pragma once
/* plot the time between write to overwrite (remove) distribution */

#include "../../traceReader/reader.h"
#include <cstdio>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../utils/include/utils.h"
#include "../struct.h"

using namespace std;

namespace analysis {
class WriteFutureReuseDistribution {
   public:
    explicit WriteFutureReuseDistribution() {
        utils::my_assert(reuse_info_array_size_ == max_reuse_rtime_ / rtime_granularity_ + fine_rtime_upbound_,
                         "reuse_info_array_size_ problem");
        reuse_info_ = new create_reuse_info_t[reuse_info_array_size_];
        memset(reuse_info_, 0, sizeof(create_reuse_info_t) * reuse_info_array_size_);
    }

    ~WriteFutureReuseDistribution() {
        delete[] reuse_info_;
    }

    inline int GET_POS_RT(int t) {
        if (t > fine_rtime_upbound_)
            return (int) (t / rtime_granularity_) + fine_rtime_upbound_;
        else
            return t;
    }

    inline int GET_RTIME_FROM_POS(int p) {
        if (p > fine_rtime_upbound_) {
            /* this is not a valid pos, because there is a gap between fine_rtime and coarse_rtime */
            if (p < fine_rtime_upbound_ + fine_rtime_upbound_ / rtime_granularity_)
                return -1;
            else
                return (p - fine_rtime_upbound_) * rtime_granularity_;
        } else {
            return p;
        }
    }

    inline void add_req(request_t *req) {
        utils::my_assert(req->next_access_vtime != -2, "trace may not be oracleReuse trace\n");
        auto it = last_write_.find(req->obj_id);

        if (it == last_write_.end()) {
            // only consider the writes in the first 3 days so that the distribution won't be distorted due to trace end
            if (req->op == OP_SET || req->op == OP_REPLACE) {
                if (req->next_access_vtime != -1 && req->next_access_vtime != INT64_MAX) {
                    ERROR("write operation but non-zero next_access_vtime %s %ld\n", req_op_str[req->op], (long) req->next_access_vtime);
                    abort();
                }
                last_write_[req->obj_id] = {(int32_t) req->clock_time, -1, 0};
            }
            return;
        }

        int rtime_since_write = (int) req->clock_time - it->second.write_rtime;
        if (rtime_since_write < 0) {
            printf("rtime since write %ld\n", (long) rtime_since_write);
            abort();
        }

        /* first 3600 use time granularity 1, after that use time_granularity_ */
        int pos_rt = GET_POS_RT(rtime_since_write);
        if (pos_rt >= reuse_info_array_size_) {
            printf("CNT_ARRAY_SIZE size too small %d %d\n", pos_rt, GET_POS_RT(rtime_since_write));
            abort();
        }

        if (req->op == OP_SET || req->op == OP_REPLACE) {
            utils::my_assert(req->next_access_vtime == -1 || req->next_access_vtime == INT64_MAX, "write request should not have reuse");
            last_write_[req->obj_id] = {(int32_t) req->clock_time, -1, 0};
        } else if (req->op == OP_DELETE) {
            last_write_.erase(req->obj_id);
            utils::my_assert(req->next_access_vtime == -1 || req->next_access_vtime == INT64_MAX, "delete request should not have reuse");
        } else if (req->op == OP_CAS) {
            ;
        }

        if (req->next_access_vtime >= 0 && req->next_access_vtime != INT64_MAX) {
            reuse_info_[pos_rt].reuse_cnt += 1;
            reuse_info_[pos_rt].reuse_freq_sum += it->second.freq;
            int32_t access_age = it->second.last_read_rtime == -1 ? 0 : (int32_t) req->clock_time - it->second.last_read_rtime;
            reuse_info_[pos_rt].reuse_access_age_sum += access_age;
        } else {
            reuse_info_[pos_rt].stop_reuse_cnt += 1;
            reuse_info_[pos_rt].stop_reuse_freq_sum += it->second.freq;
            int32_t access_age = it->second.last_read_rtime == -1 ? 0 : (int32_t) req->clock_time - it->second.last_read_rtime;
            reuse_info_[pos_rt].stop_reuse_access_age_sum += access_age;
        }

        if (req->op == OP_GET || req->op == OP_GETS || req->op == OP_INCR || req->op == OP_DECR) {
            last_write_[req->obj_id] = {it->second.write_rtime, (int32_t) req->clock_time, it->second.freq + 1};
        }
    }

    void dump(string &path_base) {
        ofstream ofs(path_base + ".writeFutureReuse", ios::out | ios::trunc);
        ofs << "# " << path_base << "\n";
        ofs << "# real time: reuse_cnt, stop_reuse_cnt, reuse_access_age_sum, stop_reuse_access_age_sum, reuse_freq_sum, stop_reuse_freq_sum"
            << "\n";
        for (int idx = 0; idx < this->reuse_info_array_size_; idx++) {
            int ts = GET_RTIME_FROM_POS(idx);
            if (ts == -1)
                continue;
            if (this->reuse_info_[idx].reuse_cnt + this->reuse_info_[idx].stop_reuse_cnt == 0)
                continue;
            ofs << ts << ":"
                << this->reuse_info_[idx].reuse_cnt << ","
                << this->reuse_info_[idx].stop_reuse_cnt << ","
                << this->reuse_info_[idx].reuse_access_age_sum << ","
                << this->reuse_info_[idx].stop_reuse_access_age_sum << ","
                << this->reuse_info_[idx].reuse_freq_sum << ","
                << this->reuse_info_[idx].stop_reuse_freq_sum << "\n";
        }
    }

   private:
    struct info_last_write {
        int32_t write_rtime;
        int32_t last_read_rtime;
        int32_t freq;
    };

    /* obj -> (write_time, read_time), read time is used to check it has been read */
    unordered_map<obj_id_t, struct info_last_write> last_write_;
    //  robin_hood::unordered_flat_map<obj_id_t, struct info_last_write> last_write_;

    /* future reuse (whether it has future request) in the struct */
    typedef struct {
        uint32_t reuse_cnt;
        uint32_t stop_reuse_cnt;
        uint64_t reuse_access_age_sum;
        uint64_t reuse_freq_sum;
        uint64_t stop_reuse_access_age_sum;
        uint64_t stop_reuse_freq_sum;
    } create_reuse_info_t;
    /* the request cnt that get a reuse or stop reuse at create age */
    create_reuse_info_t *reuse_info_;

    /* rtime smaller than fine_rtime_upbound_ will not be divide by rtime_granularity_ */
    const int fine_rtime_upbound_ = 3600;
    const int rtime_granularity_ = 5;

    const int max_reuse_rtime_ = 86400 * 120;
    const int reuse_info_array_size_ = max_reuse_rtime_ / rtime_granularity_ + fine_rtime_upbound_;
};

}// namespace analysis
