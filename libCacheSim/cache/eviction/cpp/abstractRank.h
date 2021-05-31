#pragma once

#include <vector>
#include <queue>
#include <map>


using namespace std;

namespace eviction {

using pq_pair = pair<cache_obj_t*, double>;

int cmp_pq_pair(pq_pair p1, pq_pair p2) {
  return p2.second - p1.second;
}

class abstractRank {
  /* ranking based eviction algorithm */

public:
  abstractRank() {
    ;
  }

  inline cache_obj_t *pick_lowest_score() {
    auto p = ordered_map.begin();
    cache_obj_t *obj = p->first;
    ordered_map.erase(p);

    return obj;
  }

//  priority_queue<pq_pair, vector<pq_pair>, decltype(&cmp_pq_pair)> pq;
  map<cache_obj_t*, double> ordered_map;
private:
};
}