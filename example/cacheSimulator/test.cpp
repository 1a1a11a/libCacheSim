//
// Created by Juncheng Yang on 11/15/19.
//

#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <set>
#include <tuple>

#include "libCacheSim.h"

using namespace std;

namespace eviction {
using pq_node_type = std::tuple<cache_obj_t *, double, int64_t>;
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

class abstractRank2 {
  /* ranking based eviction algorithm */

 public:
  abstractRank2() = default;

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

class LFUCpp2 : public abstractRank2 {
 public:
  LFUCpp2() = default;
  ;
};
}  // namespace eviction


int main(int argc, char **argv) {
  /* setup reader */
  reader_init_param_t init_params = default_reader_init_params();
  init_params.obj_id_field = 5;
  init_params.obj_size_field = 4;
  init_params.time_field = 2;
  init_params.has_header_set = true;
  init_params.has_header = true;
  init_params.delimiter = ',';

  reader_t *reader =
      setup_reader("../../../data/trace.csv", CSV_TRACE, &init_params);

  /* set up a request */
  request_t *req = new_request();

  /* read one request and print */
  read_one_req(reader, req);
  print_request(req);

  /* setup a cache */
  common_cache_params_t cc_params = {
      .cache_size = 1 * GiB, .hashpower = 24, .consider_obj_metadata = false};
  // cache_t *lru = LRU_init(cc_params, NULL);
  // cache_t *lru = GDSF_init(cc_params, NULL);
  cache_t *lru = LFUCpp_init(cc_params, NULL);

  // eviction::LFUCpp2 *lfu = new eviction::LFUCpp2;
  // void *ptr = lfu;
  // auto *lfu2 = static_cast<eviction::LFUCpp2 *>(ptr);

  // cache_obj_t *obj = cache_insert_base(lru, req);
  // auto itr = lfu->pq.emplace_hint(lfu->pq.begin(), obj, 1.0, lru->n_req);
  // lfu->itr_map[obj] = itr;
  // delete lfu;

  // std::vector<int> vec;
  // std::unordered_map<cache_obj_t *, std::vector<int>::iterator> itr_map{};

  // lru->get(lru, req);

  // int64_t n_hit = 0, n_req = 0;
  // while (read_one_req(reader, req) == 0) {
  //   if (lru->get(lru, req) == cache_ck_hit) {
  //     n_hit++;
  //   }
  //   n_req++;
  // }

  // printf("hit ratio: %lf\n", (double)n_hit / n_req);

  free_request(req);
  lru->cache_free(lru);
  close_reader(reader);

  return 0;
}
