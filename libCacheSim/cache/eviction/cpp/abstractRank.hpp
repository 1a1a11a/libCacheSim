#pragma once

#include <math.h>

#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "../../../dataStructure/hashtable/hashtable.h"
#include "../../../include/libCacheSim/cache.h"
#include "../../../include/libCacheSim/cacheObj.h"

using namespace std;

namespace eviction {

/* cache_obj, priority, request_vtime
 * we use request_vtime to order objects with the same priority (FIFO) **/
// using pq_node_type = tuple<cache_obj_t *, double, int64_t>;
// const pq_node_type INVALID_PQ_PAIR{nullptr, 0, -1};

struct pq_node_type {
  cache_obj_t *obj;
  double priority;
  int64_t last_request_vtime;

  pq_node_type(cache_obj_t *obj, double priority, int64_t last_request_vtime)
      : obj(obj), priority(priority), last_request_vtime(last_request_vtime){};

  void print() const {
    printf("obj %lu, priority %f, last_request_vtime %ld\n",
           (unsigned long)obj->obj_id, priority, (long)last_request_vtime);
  }

  bool operator<(const pq_node_type &rhs) const {
    if (this->priority == rhs.priority) {
      /* use FIFO when objects have the same priority */
      return this->last_request_vtime < rhs.last_request_vtime;
    }

    return this->priority < rhs.priority;
  }

  bool operator==(const pq_node_type &n) {
    return this->obj->obj_id == n.obj->obj_id;
  }
};

class abstractRank {
  /* ranking based eviction algorithm */

 public:
  abstractRank() = default;

  inline pq_node_type peek_lowest_score() {
    auto p = pq.begin();
    pq_node_type p_copy(*p);

    return std::move(p_copy);
  }

  inline pq_node_type pop_lowest_score() {
    auto p = pq.begin();
    pq_node_type p_copy(*p);
    // itr_map.erase(get<0>(*p));
    itr_map.erase(p->obj);
    pq.erase(p);

    return std::move(p_copy);
  }

  inline void remove_obj(cache_t *cache, cache_obj_t *obj) {
    auto itr = itr_map[obj];
    pq.erase(itr);
    itr_map.erase(obj);
    cache_remove_obj_base(cache, obj, true);
  }

  inline bool remove(cache_t *cache, obj_id_t obj_id) {
    cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
    if (obj == nullptr) {
      return false;
    }
    remove_obj(cache, obj);

    return true;
  }

  std::set<pq_node_type> pq{};
  std::unordered_map<cache_obj_t *, std::set<pq_node_type>::iterator> itr_map{};

 private:
};
}  // namespace eviction
