#pragma once

#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <tuple>
#include <unordered_map>
#include <unordered_set>
#include <vector>

using namespace std;

namespace eviction {

/* cache_obj, priority, request_vtime
 * we use request_vtime to order objects with the same priority, currently it is
 *FIFO
 *
 **/
using pq_node_type = tuple<cache_obj_t *, double, int64_t>;
const pq_node_type INVALID_PQ_PAIR{nullptr, 0, -1};
struct cmp_pq_node {
  bool operator()(const pq_node_type &p1, const pq_node_type &p2) const {
    if (fabs(get<1>(p1) / get<1>(p2) - 1) < 0.0001) {
      /* use FIFO when objects have the same priority */
      return get<2>(p1) < get<2>(p2);
    }
    return get<1>(p1) < get<1>(p2);
  }
};
typedef set<pq_node_type, cmp_pq_node> pq_type;

class abstractRank {
  /* ranking based eviction algorithm */

 public:
  abstractRank() = default;

  inline pq_node_type pick_lowest_score() {
    auto p = pq.begin();
    pq_node_type p_copy(*p);
    itr_map.erase(get<0>(*p));
    pq.erase(p);

    return std::move(p_copy);
  }

  inline void remove_obj(cache_t *cache, cache_obj_t *obj) {
    auto itr = itr_map[obj];
    pq.erase(itr);
    itr_map.erase(obj);
    cache_remove_obj_base(cache, obj);
  }

  inline void remove(cache_t *cache, obj_id_t obj_id) {
    cache_obj_t *obj = cache_get_obj_by_id(cache, obj_id);
    if (obj == nullptr) {
      WARN("obj is not in the cache\n");
      return;
    }
    remove_obj(cache, obj);
  }

  pq_type pq{};
  unordered_map<cache_obj_t *, pq_type::iterator> itr_map{};

 private:
};
}  // namespace eviction
