// created by Xiaojun Guo, 03/01/25. Currently, it is used in mrcProfiler to
// sample fixed number of objects with smallest hash value

#ifndef INCLUDE_MIN_VALUE_MAP_HPP
#define INCLUDE_MIN_VALUE_MAP_HPP

#include <set>
#include <unordered_map>

template <typename K, typename V>
class MinValueMap {
 public:
  MinValueMap(size_t n_param) : n(n_param) {}

  bool find(const K &key) { return map.count(key); }

  K insert(const K &key, const V &value, bool &poped) {
    poped = false;
    auto it = map.find(key);
    if (it != map.end()) {
      // Key already exists, update its value
      auto setIt = set.find({it->second, it->first});
      set.erase(setIt);
      set.insert({value, key});
      it->second = value;
    } else {
      // New key
      map[key] = value;
      if (set.size() < n) {
        set.insert({value, key});
      } else if (value < set.rbegin()->first) {
        auto last = *set.rbegin();
        set.erase(last);
        set.insert({value, key});
        map.erase(last.second);
        poped = true;
        return last.second;
      }
    }
    return -1;
  }

  bool full() const { return set.size() == n; }

  bool empty() const { return set.empty(); }

  V get_max_value() const { return set.rbegin()->first; }

  size_t n;
  std::set<std::pair<V, K>> set;
  std::unordered_map<K, V> map;

 private:
};

#endif