// created by Xiaojun Guo, 02/28/25

#include <stdint.h>
#include "../../../dataStructure/segment_tree.hpp"

class BeladyOnline {
    int64_t cache_size_;
    SegmentTree tree;
    unordered_map<int64_t, int64_t> last_access_vtime_;
    int64_t v_time_;

public:
    BeladyOnline(int64_t cache_size) {
        cache_size_ = cache_size;
        v_time_ = 0;
    }


    bool BeladyOnline_get(int64_t obj_id, int64_t size){
        bool ret = false;
        bool accessed_before = false;
        int64_t last_access_vtime = 0;

        /* 1. get the last access time */
        if(last_access_vtime_.count(obj_id)){
            accessed_before = true;
            last_access_vtime = last_access_vtime_[obj_id];
        }


        /* 2. decide whether it is a hit */
        if(accessed_before){
            int64_t current_occ = tree.query(last_access_vtime, v_time_);
            if((current_occ + size) <= cache_size_){
                tree.update(last_access_vtime, v_time_, size);
                ret = true;
            }
        }
        
        /* 3. update the last access time */
        last_access_vtime_[obj_id] = v_time_;
        
        /* 4. update the v_time_ */
        v_time_ ++;

        return ret;
    }

};

extern "C" {

    BeladyOnline* BeladyOnline_new(int64_t cache_size) { return new BeladyOnline(cache_size); }
    bool _BeladyOnline_get(BeladyOnline* obj, int64_t obj_id, int64_t size) { return obj->BeladyOnline_get(obj_id, size); }
    void BeladyOnline_delete(BeladyOnline* obj) { delete obj; }
}