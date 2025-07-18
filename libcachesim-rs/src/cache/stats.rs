//! Cache statistics and metrics

/// Cache performance statistics
///
/// This struct contains various metrics about cache performance,
/// including hit rates, miss rates, and utilization information.
#[derive(Debug, Clone, PartialEq)]
pub struct CacheStats {
    /// Total number of requests processed
    pub requests: u64,
    /// Number of cache hits
    pub hits: u64,
    /// Number of cache misses
    pub misses: u64,
    /// Hit rate as a fraction (0.0 to 1.0)
    pub hit_rate: f64,
    /// Miss rate as a fraction (0.0 to 1.0)
    pub miss_rate: f64,
    /// Number of objects currently in cache
    pub objects: u64,
    /// Bytes currently occupied in cache
    pub occupied_bytes: u64,
    /// Total cache capacity in bytes
    pub capacity: u64,
}

impl CacheStats {
    /// Create new statistics with zero values
    pub fn new(capacity: u64) -> Self {
        Self {
            requests: 0,
            hits: 0,
            misses: 0,
            hit_rate: 0.0,
            miss_rate: 0.0,
            objects: 0,
            occupied_bytes: 0,
            capacity,
        }
    }

    /// Get hit ratio (same as hit_rate, for convenience)
    pub fn hit_ratio(&self) -> f64 {
        self.hit_rate
    }

    /// Get miss ratio (same as miss_rate, for convenience)
    pub fn miss_ratio(&self) -> f64 {
        self.miss_rate
    }

    /// Get cache utilization as a fraction (0.0 to 1.0)
    pub fn utilization(&self) -> f64 {
        if self.capacity == 0 {
            0.0
        } else {
            self.occupied_bytes as f64 / self.capacity as f64
        }
    }

    /// Get cache utilization as a percentage (0.0 to 100.0)
    pub fn utilization_percent(&self) -> f64 {
        self.utilization() * 100.0
    }

    /// Get hit rate as a percentage (0.0 to 100.0)
    pub fn hit_rate_percent(&self) -> f64 {
        self.hit_rate * 100.0
    }

    /// Get miss rate as a percentage (0.0 to 100.0)
    pub fn miss_rate_percent(&self) -> f64 {
        self.miss_rate * 100.0
    }

    /// Get average object size in bytes
    pub fn average_object_size(&self) -> f64 {
        if self.objects == 0 {
            0.0
        } else {
            self.occupied_bytes as f64 / self.objects as f64
        }
    }

    /// Check if the cache is empty
    pub fn is_empty(&self) -> bool {
        self.objects == 0
    }

    /// Check if the cache is full (utilization >= 100%)
    pub fn is_full(&self) -> bool {
        self.occupied_bytes >= self.capacity
    }

    /// Get remaining capacity in bytes
    pub fn remaining_capacity(&self) -> u64 {
        self.capacity.saturating_sub(self.occupied_bytes)
    }
}

impl Default for CacheStats {
    fn default() -> Self {
        Self::new(0)
    }
}

impl std::fmt::Display for CacheStats {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(
            f,
            "CacheStats {{ requests: {}, hits: {}, misses: {}, hit_rate: {:.2}%, utilization: {:.2}%, objects: {} }}",
            self.requests,
            self.hits,
            self.misses,
            self.hit_rate_percent(),
            self.utilization_percent(),
            self.objects
        )
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cache_stats_new() {
        let stats = CacheStats::new(1000);
        assert_eq!(stats.capacity, 1000);
        assert_eq!(stats.requests, 0);
        assert_eq!(stats.hits, 0);
        assert_eq!(stats.misses, 0);
        assert_eq!(stats.hit_rate, 0.0);
        assert_eq!(stats.miss_rate, 0.0);
    }

    #[test]
    fn test_utilization() {
        let stats = CacheStats {
            capacity: 1000,
            occupied_bytes: 500,
            ..CacheStats::new(1000)
        };

        assert_eq!(stats.utilization(), 0.5);
        assert_eq!(stats.utilization_percent(), 50.0);
        assert_eq!(stats.remaining_capacity(), 500);
    }

    #[test]
    fn test_hit_rate_percent() {
        let stats = CacheStats {
            hit_rate: 0.75,
            miss_rate: 0.25,
            ..CacheStats::new(1000)
        };

        assert_eq!(stats.hit_rate_percent(), 75.0);
        assert_eq!(stats.miss_rate_percent(), 25.0);
    }

    #[test]
    fn test_average_object_size() {
        let stats = CacheStats {
            objects: 10,
            occupied_bytes: 1000,
            ..CacheStats::new(2000)
        };

        assert_eq!(stats.average_object_size(), 100.0);
    }

    #[test]
    fn test_is_empty_and_full() {
        let empty_stats = CacheStats::new(1000);
        assert!(empty_stats.is_empty());
        assert!(!empty_stats.is_full());

        let full_stats = CacheStats {
            occupied_bytes: 1000,
            capacity: 1000,
            objects: 10,
            ..CacheStats::new(1000)
        };
        assert!(!full_stats.is_empty());
        assert!(full_stats.is_full());
    }

    #[test]
    fn test_display() {
        let stats = CacheStats {
            requests: 100,
            hits: 75,
            misses: 25,
            hit_rate: 0.75,
            objects: 50,
            occupied_bytes: 500,
            capacity: 1000,
            ..CacheStats::new(1000)
        };

        let display = format!("{}", stats);
        assert!(display.contains("requests: 100"));
        assert!(display.contains("hits: 75"));
        assert!(display.contains("hit_rate: 75.00%"));
        assert!(display.contains("utilization: 50.00%"));
    }
}
