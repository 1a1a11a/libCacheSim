
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
#include "../lib/robin_hood.h"
#include "../struct.h"

using namespace std;

namespace analysis {
class WriteReuseDistribution {
   public:
    explicit WriteReuseDistribution() {
        utils::my_assert(reuse_info_array_size_ == max_reuse_rtime_ / rtime_granularity_ + fine_rtime_upbound_,
                         "reuse_info_array_size_ problem");
        read_reuse_cnt_ = new uint32_t[reuse_info_array_size_];
        write_reuse_cnt_ = new uint32_t[reuse_info_array_size_];
        remove_reuse_cnt_ = new uint32_t[reuse_info_array_size_];
        memset(read_reuse_cnt_, 0, sizeof(uint32_t) * reuse_info_array_size_);
        memset(write_reuse_cnt_, 0, sizeof(uint32_t) * reuse_info_array_size_);
        memset(remove_reuse_cnt_, 0, sizeof(uint32_t) * reuse_info_array_size_);
    };

    ~WriteReuseDistribution() {
        delete[] read_reuse_cnt_;
        delete[] write_reuse_cnt_;
        delete[] remove_reuse_cnt_;
    };

    inline int get_pos_from_rtime(int t) const {
        if (t > fine_rtime_upbound_)
            return (int) (t / rtime_granularity_) + fine_rtime_upbound_;
        else
            return t;
    }

    inline int get_rtime_from_pos(int p) const {
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
        auto it = last_write_rtime_.find(req->obj_id);

        if (it == last_write_rtime_.end()) {
            if (req->op == OP_SET || req->op == OP_ADD || req->op == OP_REPLACE || req->op == OP_CAS || req->op == OP_WRITE) {
                last_write_rtime_[req->obj_id] = (uint32_t) req->clock_time;
            }
            return;
        }

        int rtime_since_write = (int) req->clock_time - it->second;
        last_write_rtime_.erase(it);
        utils::my_assert(rtime_since_write >= 0, "rtime since write <0");

        int pos_rt = get_pos_from_rtime(rtime_since_write);
        utils::my_assert(pos_rt < reuse_info_array_size_, "CNT_ARRAY_SIZE size too small\n");

        if (req->op == OP_GET || req->op == OP_GETS || req->op == OP_INCR || req->op == OP_DECR) {
            read_reuse_cnt_[pos_rt]++;
        } else if (req->op == OP_SET || req->op == OP_ADD || req->op == OP_REPLACE || req->op == OP_CAS) {
            write_reuse_cnt_[pos_rt]++;
            last_write_rtime_[req->obj_id] = (uint32_t) req->clock_time;
        } else if (req->op == OP_DELETE) {
            remove_reuse_cnt_[pos_rt]++;
        } else {
            //      ERROR("op error %d\n", req->op);
        }
    }

    void dump(string &path_base) {
        ofstream ofs(path_base + ".writeReuse", ios::out | ios::trunc);
        ofs << "# " << path_base << "\n";
        ofs << "# read reuse real time: req_cnt\n";
        for (int idx = 0; idx < reuse_info_array_size_; idx++) {
            uint32_t n_read = read_reuse_cnt_[idx];
            uint32_t n_write = write_reuse_cnt_[idx];
            uint32_t n_remove = remove_reuse_cnt_[idx];
            if (n_read + n_write + n_remove > 0) {
                int rtime = get_rtime_from_pos(idx);
                ofs << rtime << ":" << n_read << "," << n_write << "," << n_remove << "\n";
            }
        }
        ofs.close();
    }

   private:
    /* request count for reuse rtime/vtime */
    //  typedef struct {
    //    int64_t read_freq_sum;
    //    int32_t read_cnt;
    //    int32_t first_read_cnt;
    //    int32_t write_cnt;
    //    int32_t remove_cnt;
    //  } write_reuse_cnt_t;
    /* at each rtime, the reuse request count */
    //  write_reuse_cnt_t *reuse_info_;

    //  struct info_last_write {
    //    int32_t write_rtime;
    //    int32_t last_read_rtime;
    //    int32_t freq;
    //  };

    /* obj -> (write_time, read_time), read time is used to check it has been read */
    //  robin_hood::unordered_flat_map<obj_id_t, struct info_last_write> last_write_;

    robin_hood::unordered_flat_map<obj_id_t, uint32_t> last_write_rtime_;
    uint32_t *read_reuse_cnt_;// only consider the first reuse
    uint32_t *write_reuse_cnt_;
    uint32_t *remove_reuse_cnt_;

    //  int32_t last_rtime_;

    /* rtime smaller than fine_rtime_upbound_ will not be divide by rtime_granularity_ */
    const int fine_rtime_upbound_ = 3600;
    const int rtime_granularity_ = 5;

    const int max_reuse_rtime_ = 86400 * 80;
    const int reuse_info_array_size_ = max_reuse_rtime_ / rtime_granularity_ + fine_rtime_upbound_;
};
}// namespace analysis
