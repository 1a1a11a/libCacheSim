use std::num::NonZeroUsize;

use crate::Key;

use super::EvictPolicy;

// LRU (Least Recently Used) Policy implementation
pub struct LruPolicy {
    capacity: u64,
    size: u64,
    cache: lru::LruCache<Key, u64>,
}

impl EvictPolicy for LruPolicy {
    fn new(capacity: u64) -> Self {
        Self {
            capacity,
            size: 0,
            cache: lru::LruCache::new(NonZeroUsize::new(capacity as usize).unwrap()),
        }
    }

    fn get(&mut self, key: Key) -> Option<()> {
        self.cache.get(&key).map(|_| ())
    }

    fn put(&mut self, key: Key, size: u64) {
        // Evict items if necessary
        while self.size + size > self.capacity {
            if let Some((_, evicted_size)) = self.cache.pop_lru() {
                self.size -= evicted_size;
            } else {
                break;
            }
        }
        self.cache.put(key, size);
        self.size += size;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_lru_policy() {
        let mut lru = LruPolicy::new(10);

        // Fill the cache
        lru.put(1, 2);
        lru.put(2, 3);
        lru.put(3, 5);

        // Access 1, making it the most recently used
        assert!(lru.get(1).is_some());

        // This should evict 2, not 1
        lru.put(4, 3);

        assert!(lru.get(1).is_some());
        assert!(lru.get(3).is_some());
        assert!(lru.get(4).is_some());
        assert!(lru.get(2).is_none());

        // Access 3, making it the most recently used
        assert!(lru.get(3).is_some());

        // This should evict 1, the least recently used
        lru.put(5, 2);

        assert!(lru.get(3).is_some());
        assert!(lru.get(4).is_some());
        assert!(lru.get(5).is_some());
        assert!(lru.get(1).is_none());
    }
}
