#pragma once

#include <vector>
#include <queue>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <fstream>
#include <iostream>
#include <tuple>


using namespace std;

namespace eviction {

/* cache_obj, priority, request_vtime
 * we include request_vtime because of two reasons
 * 1. because we cannot update the priority in priority queue,
 *    request_vtime is used to check whether the entry in the priority queue
 *    is most up to date
 * 2. we use request_vtime to order objects with the same priority, currently it is FIFO
 *
 **/
using pq_pair = tuple<cache_obj_t*, double, int64_t>;
const pq_pair INVALID_PQ_PAIR{nullptr, 0, -1};
struct cmp_pq_pair {
  bool operator() (const pq_pair &p1, const pq_pair &p2) {
    if (abs(get<1>(p1) / get<1>(p2) - 1) < 0.0001) {
      /* use FIFO when objects have the same priority */
      return get<2>(p1) > get<2>(p2);
    }
    return get<1>(p1) > get<1>(p2);
  }
};

class abstractRank {
  /* ranking based eviction algorithm */

public:
  abstractRank() {
    ;
  }

  inline pq_pair pick_lowest_score() {
    if (unlikely(pq.empty()))
      return INVALID_PQ_PAIR;

    pq_pair p;

    do {
    /* a pq pair can be invalid if its priority has changed,
     * or the object has been deleted */
      p = pq.top();
      pq.pop();
    } while (!is_valid_pq_pair(p) && !pq.empty());

    if (unlikely(pq.empty())) {
      return INVALID_PQ_PAIR;
    }

    return p;
  }

  inline void remove_obj(cache_t *cache, cache_obj_t *obj) {
    deleted_obj.insert(obj);
    cache_remove_obj_base(cache, obj);
  }

  inline void remove(cache_t *cache, obj_id_t obj_id) {
    cache_obj_t *obj = hashtable_find_obj_id(cache->hashtable, obj_id);
    if (obj == nullptr) {
      WARNING("obj is not in the cache\n");
      return;
    }
    remove_obj(cache, obj);
  }

//  void _temp() {
//    typedef bool (*pq_cmp_func)(const pq_pair&, const pq_pair&);
//    /* a priority queue for ordering the objects in cache */
//    pq_cmp_func cmp =
//        [](const pq_pair &p1, const pq_pair &p2) { return p2.second > p1.second; };
//    priority_queue<pq_pair, vector<pq_pair>, decltype(cmp)> pq(cmp);
//    priority_queue<pq_pair, vector<pq_pair>, std::function<bool(const pq_pair&, const pq_pair&)>> pq(cmp);
//  }

  priority_queue<pq_pair, vector<pq_pair>, cmp_pq_pair> pq;
  uint64_t vtime = 0; /* the number of requests */
//  unordered_map<cache_obj_t *, double> latest_pri;
private:
  inline bool is_valid_pq_pair(pq_pair p) {
    cache_obj_t *obj = get<0>(p);
    if (deleted_obj.size() != 0 && deleted_obj.find(obj) != deleted_obj.end()) {
      deleted_obj.erase(obj);
      return false;
    }

    if (get<0>(p)->rank.last_access_vtime != get<2>(p)) {
      return false;
    }

    return true;
  }

  /* because we cannot remove entries from priority queue,
   * we add a tombstone each time we need to delete an entry
   * and remove the entry when it pops up in the priority queue,
   * this trades memory for speed */
  unordered_set<cache_obj_t *> deleted_obj;
};
}

