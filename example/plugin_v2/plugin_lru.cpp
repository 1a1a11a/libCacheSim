//
// LRU Cache Plugin with Hooks for libCacheSim plugin_cache.c
// This implements the hook-based plugin system that plugin_cache.c expects
//

#include <glib.h>
#include <libCacheSim.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>

#include <unordered_map>

class StandaloneLRU {
 public:
  struct Node {
    obj_id_t obj_id;
    uint64_t obj_size;
    Node *prev;
    Node *next;

    Node(obj_id_t k = 0, uint64_t obj_size = 0)
        : obj_id(k), obj_size(obj_size), prev(nullptr), next(nullptr) {}
  };

  std::unordered_map<obj_id_t, Node *> cache_map;
  Node *head;
  Node *tail;

  StandaloneLRU() {
    head = new Node();
    tail = new Node();
    head->next = tail;
    tail->prev = head;
  }

  ~StandaloneLRU() {
    while (head) {
      Node *temp = head;
      head = head->next;
      delete temp;
    }
  }

  void add_to_head(Node *node) {
    node->prev = head;
    node->next = head->next;
    head->next->prev = node;
    head->next = node;
  }

  void remove_node(Node *node) {
    node->prev->next = node->next;
    node->next->prev = node->prev;
  }

  Node *remove_tail() {
    Node *last_node = tail->prev;
    remove_node(last_node);
    return last_node;
  }

  void move_to_head(Node *node) {
    remove_node(node);
    add_to_head(node);
  }

  void cache_hit(obj_id_t obj_id) {
    Node *node = cache_map[obj_id];
    move_to_head(node);
  }

  void cache_miss(obj_id_t obj_id, uint64_t obj_size) {
    Node *new_node = new Node(obj_id, obj_size);
    cache_map[obj_id] = new_node;
    add_to_head(new_node);
  }

  obj_id_t cache_eviction() {
    Node *node = remove_tail();
    obj_id_t evicted_id = node->obj_id;
    cache_map.erase(evicted_id);
    delete node;
    return evicted_id;
  }

  void cache_remove(obj_id_t obj_id) {
    auto it = cache_map.find(obj_id);
    if (it == cache_map.end()) {
      return;
    }
    Node *node = it->second;
    remove_node(node);
    cache_map.erase(it);
    delete node;
  }
};

// C interface for the plugin hooks
extern "C" {

// implement the cache init hook
void *cache_init_hook(const common_cache_params_t ccache_params) {
  // initialize the LRU cache
  StandaloneLRU *lru_cache = new StandaloneLRU();
  return lru_cache;
}

// implement the cache hit hook
void cache_hit_hook(void *data, const request_t *req) {
  // move object to the head of the list
  StandaloneLRU *lru_cache = (StandaloneLRU *)data;
  lru_cache->cache_hit(req->obj_id);
}

// implement the cache miss hook
void cache_miss_hook(void *data, const request_t *req) {
  // insert object into the cache
  StandaloneLRU *lru_cache = (StandaloneLRU *)data;
  lru_cache->cache_miss(req->obj_id, req->obj_size);
}

// implement the cache eviction hook
obj_id_t cache_eviction_hook(void *data, const request_t *req) {
  // evict the least recently used object
  StandaloneLRU *lru_cache = (StandaloneLRU *)data;
  return lru_cache->cache_eviction();
}

// implement the cache remove hook
void cache_remove_hook(void *data, const obj_id_t obj_id) {
  // remove object from the cache
  StandaloneLRU *lru_cache = (StandaloneLRU *)data;
  lru_cache->cache_remove(obj_id);
}

// implement the cache free hook
void cache_free_hook(void *data) {
  // free the LRU cache (destructor handles all cleanup)
  StandaloneLRU *lru_cache = (StandaloneLRU *)data;
  delete lru_cache;
}

}  // extern "C"
