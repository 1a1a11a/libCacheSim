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

#include "dataStructure/hashtable/hashtable.h"
#include "libCacheSim/cache.h"
#include "libCacheSim/cacheObj.h"

using namespace std;

namespace eviction {

/* cache_obj, priority, request_vtime
 * we use request_vtime to order objects with the same priority (FIFO) **/
struct pq_node_type {
  cache_obj_t *obj;
  double priority;
  int64_t last_request_vtime;

  pq_node_type() {
    obj = nullptr;
    priority = -1;
    last_request_vtime = -1;
  };

  pq_node_type(cache_obj_t *obj_param, double priority_param,
               int64_t last_request_vtime_param)
      : obj(obj_param),
        priority(priority_param),
        last_request_vtime(last_request_vtime_param){};

  void print() const {
    printf("obj %lu, priority %f, last_request_vtime %ld\n",
           (unsigned long)obj->obj_id, priority, (long)last_request_vtime);
  }

  bool operator<(const pq_node_type &rhs) const {
    DEBUG_ASSERT(this->last_request_vtime != rhs.last_request_vtime ||
                 this->obj->obj_id == rhs.obj->obj_id);
    if (this->priority == rhs.priority) {
      /* use FIFO when objects have the same priority */
      return this->last_request_vtime < rhs.last_request_vtime;
    }

    return this->priority < rhs.priority;
  }
};

class abstractRank {
  /* ranking based eviction algorithm */

 public:
  abstractRank() = default;

  inline pq_node_type peek_lowest_score() {
    auto p = pq.begin();
    pq_node_type p_copy(*p);

    return p_copy;
  }

  inline pq_node_type pop_lowest_score() {
    auto p = pq.begin();
    pq_node_type p_copy(*p);
    pq_map.erase(p->obj);
    pq.erase(p);

    return p_copy;
  }

  inline void remove_obj(cache_t *cache, cache_obj_t *obj) {
    auto pq_node = pq_map[obj];
    pq.erase(pq_node);
    pq_map.erase(obj);
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

  void print_keys() {
    printf("pq size %ld, pq_map size %ld\n", pq.size(), pq_map.size());
    printf("============= pq =============\n");
    for (auto &p : pq) {
      p.print();
    }
    printf("============= pq_map =============\n");
    for (auto &p : pq_map) {
      printf("key %lu, ", (unsigned long)p.first->obj_id);
      p.second.print();
    }
  }

  std::set<pq_node_type> pq{};
  std::unordered_map<cache_obj_t *, pq_node_type> pq_map;

 private:
};
}  // namespace eviction
