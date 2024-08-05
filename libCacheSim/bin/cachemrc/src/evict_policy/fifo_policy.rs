use std::collections::VecDeque;

use hashbrown::HashMap;

use crate::Key;

use super::EvictPolicy;

// FIFO (First In First Out) Policy implementation
pub struct FifoPolicy {
    capacity: u64,
    size: u64,
    cache: HashMap<Key, u64>,
    queue: VecDeque<Key>,
}

impl EvictPolicy for FifoPolicy {
    fn new(capacity: u64) -> Self {
        Self {
            capacity,
            size: 0,
            cache: HashMap::new(),
            queue: VecDeque::new(),
        }
    }

    fn get(&mut self, key: Key) -> Option<()> {
        self.cache.get(&key).map(|_| ())
    }

    fn put(&mut self, key: Key, size: u64) {
        // Evict items if necessary
        while self.size + size > self.capacity {
            if let Some(old_key) = self.queue.pop_front() {
                if let Some(old_size) = self.cache.remove(&old_key) {
                    self.size -= old_size;
                }
            } else {
                break; // Prevent infinite loop
            }
        }

        self.cache.insert(key, size);
        self.queue.push_back(key);
        self.size += size;
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_get_existing_key() {
        let mut policy = FifoPolicy::new(100);
        policy.put(1, 10);
        assert!(policy.get(1).is_some());
    }

    #[test]
    fn test_get_non_existing_key() {
        let mut policy = FifoPolicy::new(100);
        assert!(policy.get(1).is_none());
    }

    #[test]
    fn test_put_within_capacity() {
        let mut policy = FifoPolicy::new(100);
        policy.put(1, 50);
        policy.put(2, 30);
        assert_eq!(policy.size, 80);
        assert_eq!(policy.cache.len(), 2);
        assert_eq!(policy.queue.len(), 2);
    }

    #[test]
    fn test_put_exceeding_capacity() {
        let mut policy = FifoPolicy::new(100);
        policy.put(1, 60);
        policy.put(2, 50);
        policy.put(3, 30);
        assert_eq!(policy.size, 80);
        assert_eq!(policy.cache.len(), 2);
        assert_eq!(policy.queue.len(), 2);
        assert!(policy.get(1).is_none());
        assert!(policy.get(2).is_some());
        assert!(policy.get(3).is_some());
    }

    #[test]
    fn test_put_large_item() {
        let mut policy = FifoPolicy::new(100);
        policy.put(1, 30);
        policy.put(2, 40);
        policy.put(3, 90);
        assert_eq!(policy.size, 90);
        assert_eq!(policy.cache.len(), 1);
        assert_eq!(policy.queue.len(), 1);
        assert!(policy.get(1).is_none());
        assert!(policy.get(2).is_none());
        assert!(policy.get(3).is_some());
    }

    #[test]
    fn test_fifo_order() {
        let mut policy = FifoPolicy::new(100);
        policy.put(1, 30);
        policy.put(2, 30);
        policy.put(3, 30);
        policy.put(4, 30);
        assert_eq!(policy.size, 90);
        assert_eq!(policy.cache.len(), 3);
        assert_eq!(policy.queue.len(), 3);
        assert!(policy.get(1).is_none());
        assert!(policy.get(2).is_some());
        assert!(policy.get(3).is_some());
        assert!(policy.get(4).is_some());
    }
}
