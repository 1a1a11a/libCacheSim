use crate::Key;

mod fifo_policy;
mod lru_policy;

pub use fifo_policy::FifoPolicy;
pub use lru_policy::LruPolicy;

// Define the EvictPolicy trait
/// Defines the `EvictPolicy` trait for cache eviction policies.
pub trait EvictPolicy: Send {
    /// Creates a new instance of the eviction policy with the specified capacity.
    ///
    /// # Arguments
    ///
    /// * `capacity` - The maximum capacity of the cache.
    ///
    /// # Returns
    ///
    /// A new instance of the eviction policy.
    fn new(capacity: u64) -> Self;

    /// Retrieves the value associated with the given key from the cache.
    ///
    /// # Arguments
    ///
    /// * `key` - The key to retrieve the value for.
    ///
    /// # Returns
    ///
    /// An optional value representing the retrieved value, or `None` if the key is not found.
    fn get(&mut self, key: Key) -> Option<()>;

    /// Inserts a key-value pair into the cache.
    ///
    /// # Arguments
    ///
    /// * `key` - The key to insert.
    /// * `size` - The size of the value associated with the key.
    fn put(&mut self, key: Key, size: u64);
}
