#pragma once

#include <vector>
#include <queue>


using namespace std;

namespace eviction {
using pq_pair = pair<cache_obj_t*, double>;
int cmp_pq_pair(pq_pair p1, pq_pair p2) {
  return p2.second - p1.second;
}

class abstractRank {
  /* ranking based eviction algorithm */

public:
  inline cache_obj_t *pick_lowest_score() {
    cache_obj_t *obj = pq.top().first;
    pq.pop();
    return obj;
  }

private:
  priority_queue<pq_pair, vector<pq_pair>, decltype(&cmp_pq_pair)> pq;
};
}