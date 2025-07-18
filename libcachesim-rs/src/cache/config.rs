//! Cache configuration types and key definitions

use crate::error::{CacheError, Result};
use std::time::Duration;

/// Cache configuration parameters
#[derive(Debug, Clone)]
pub struct CacheConfig {
    /// Maximum cache capacity in bytes
    pub capacity: u64,
    /// Default TTL for cache entries
    pub default_ttl: Option<Duration>,
    /// Whether to consider metadata in size calculations
    pub consider_metadata: bool,
    /// Hash table power (affects hash table size: 2^hash_power)
    pub hash_power: u32,
}

impl Default for CacheConfig {
    fn default() -> Self {
        Self {
            capacity: 1024 * 1024 * 1024, // 1GB default
            default_ttl: Some(Duration::from_secs(86400 * 365)), // 1 year
            consider_metadata: false,
            hash_power: 20, // 2^20 = ~1M hash table entries
        }
    }
}

impl CacheConfig {
    /// Create a new cache configuration with validation
    ///
    /// # Arguments
    ///
    /// * `capacity` - Cache capacity in bytes (must be > 0)
    /// * `default_ttl` - Default TTL for cache entries
    /// * `consider_metadata` - Whether to consider metadata in size calculations
    /// * `hash_power` - Hash table power (must be between 10 and 30)
    ///
    /// # Errors
    ///
    /// Returns `CacheError::InvalidConfiguration` if parameters are invalid.
    pub fn new(
        capacity: u64,
        default_ttl: Option<Duration>,
        consider_metadata: bool,
        hash_power: u32,
    ) -> Result<Self> {
        let config = Self {
            capacity,
            default_ttl,
            consider_metadata,
            hash_power,
        };
        config.validate()?;
        Ok(config)
    }

    /// Validate the cache configuration parameters
    ///
    /// # Errors
    ///
    /// Returns `CacheError::InvalidConfiguration` if any parameter is invalid.
    pub fn validate(&self) -> Result<()> {
        if self.capacity == 0 {
            return Err(CacheError::invalid_configuration(
                "Cache capacity must be greater than 0",
            ));
        }

        if self.hash_power < 10 || self.hash_power > 30 {
            return Err(CacheError::invalid_configuration(
                "Hash power must be between 10 and 30",
            ));
        }

        if let Some(ttl) = self.default_ttl {
            if ttl.as_secs() == 0 {
                return Err(CacheError::invalid_configuration(
                    "Default TTL must be greater than 0 seconds",
                ));
            }
        }

        Ok(())
    }

    /// Convert to libCacheSim's common_cache_params_t structure
    ///
    /// This function converts the Rust configuration to the C structure
    /// expected by libCacheSim cache initialization functions.
    pub fn to_common_cache_params(&self) -> crate::ffi::sys::common_cache_params_t {
        crate::ffi::sys::common_cache_params_t {
            cache_size: self.capacity,
            default_ttl: self
                .default_ttl
                .map(|ttl| ttl.as_secs())
                .unwrap_or(86400 * 365), // Default to 1 year if None
            hashpower: self.hash_power as i32,
            consider_obj_metadata: self.consider_metadata,
        }
    }

    /// Create a CacheConfig from libCacheSim's common_cache_params_t
    ///
    /// This function converts from the C structure to the Rust configuration.
    pub fn from_common_cache_params(params: &crate::ffi::sys::common_cache_params_t) -> Self {
        Self {
            capacity: params.cache_size,
            default_ttl: if params.default_ttl > 0 {
                Some(Duration::from_secs(params.default_ttl))
            } else {
                None
            },
            consider_metadata: params.consider_obj_metadata,
            hash_power: params.hashpower as u32,
        }
    }

    /// Create a builder for constructing cache configurations
    pub fn builder() -> CacheConfigBuilder {
        CacheConfigBuilder::new()
    }
}

/// Builder for constructing CacheConfig instances
#[derive(Debug, Default)]
pub struct CacheConfigBuilder {
    capacity: Option<u64>,
    default_ttl: Option<Duration>,
    consider_metadata: Option<bool>,
    hash_power: Option<u32>,
}

impl CacheConfigBuilder {
    /// Create a new builder
    pub fn new() -> Self {
        Self::default()
    }

    /// Set the cache capacity in bytes
    pub fn capacity(mut self, capacity: u64) -> Self {
        self.capacity = Some(capacity);
        self
    }

    /// Set the cache capacity in megabytes
    pub fn capacity_mb(mut self, mb: u64) -> Self {
        self.capacity = Some(mb * 1024 * 1024);
        self
    }

    /// Set the cache capacity in gigabytes
    pub fn capacity_gb(mut self, gb: u64) -> Self {
        self.capacity = Some(gb * 1024 * 1024 * 1024);
        self
    }

    /// Set the default TTL
    pub fn default_ttl(mut self, ttl: Duration) -> Self {
        self.default_ttl = Some(ttl);
        self
    }

    /// Set the default TTL in seconds
    pub fn default_ttl_secs(mut self, secs: u64) -> Self {
        self.default_ttl = Some(Duration::from_secs(secs));
        self
    }

    /// Set whether to consider metadata in size calculations
    pub fn consider_metadata(mut self, consider: bool) -> Self {
        self.consider_metadata = Some(consider);
        self
    }

    /// Set the hash power
    pub fn hash_power(mut self, power: u32) -> Self {
        self.hash_power = Some(power);
        self
    }

    /// Build the configuration with validation
    pub fn build(self) -> Result<CacheConfig> {
        let config = CacheConfig {
            capacity: self.capacity.unwrap_or(1024 * 1024 * 1024), // 1GB default
            default_ttl: self.default_ttl.or_else(|| Some(Duration::from_secs(86400 * 365))), // 1 year default
            consider_metadata: self.consider_metadata.unwrap_or(false),
            hash_power: self.hash_power.unwrap_or(20),
        };
        config.validate()?;
        Ok(config)
    }
}

/// Cache key types supported by libCacheSim
#[derive(Debug, Clone, PartialEq, Eq, Hash)]
pub enum CacheKey {
    /// Numeric key (most common and efficient)
    Numeric(u64),
    /// String key
    String(String),
    /// Raw bytes key
    Bytes(Vec<u8>),
}

impl From<u64> for CacheKey {
    fn from(value: u64) -> Self {
        CacheKey::Numeric(value)
    }
}

impl From<String> for CacheKey {
    fn from(value: String) -> Self {
        CacheKey::String(value)
    }
}

impl From<&str> for CacheKey {
    fn from(value: &str) -> Self {
        CacheKey::String(value.to_string())
    }
}

impl From<Vec<u8>> for CacheKey {
    fn from(value: Vec<u8>) -> Self {
        CacheKey::Bytes(value)
    }
}

impl From<&[u8]> for CacheKey {
    fn from(value: &[u8]) -> Self {
        CacheKey::Bytes(value.to_vec())
    }
}

impl CacheKey {
    /// Convert to libCacheSim's obj_id_t
    ///
    /// This function converts the Rust CacheKey to the C obj_id_t type
    /// used by libCacheSim. For numeric keys, this is a direct conversion.
    /// For string and byte keys, we compute a hash.
    pub fn to_obj_id(&self) -> crate::ffi::sys::obj_id_t {
        match self {
            CacheKey::Numeric(id) => *id,
            CacheKey::String(s) => {
                // Use a simple hash function for string keys
                // In a real implementation, you might want to use a more sophisticated hash
                use std::collections::hash_map::DefaultHasher;
                use std::hash::{Hash, Hasher};

                let mut hasher = DefaultHasher::new();
                s.hash(&mut hasher);
                hasher.finish()
            }
            CacheKey::Bytes(bytes) => {
                // Use a simple hash function for byte keys
                use std::collections::hash_map::DefaultHasher;
                use std::hash::{Hash, Hasher};

                let mut hasher = DefaultHasher::new();
                bytes.hash(&mut hasher);
                hasher.finish()
            }
        }
    }

    /// Create a CacheKey from libCacheSim's obj_id_t
    ///
    /// This creates a numeric cache key from the C obj_id_t.
    /// Note that this loses information for keys that were originally strings or bytes.
    pub fn from_obj_id(obj_id: crate::ffi::sys::obj_id_t) -> Self {
        CacheKey::Numeric(obj_id)
    }

    /// Get the size of the key in bytes (for memory accounting)
    pub fn size_bytes(&self) -> usize {
        match self {
            CacheKey::Numeric(_) => std::mem::size_of::<u64>(),
            CacheKey::String(s) => s.len(),
            CacheKey::Bytes(bytes) => bytes.len(),
        }
    }

    /// Check if this key is numeric (most efficient for libCacheSim)
    pub fn is_numeric(&self) -> bool {
        matches!(self, CacheKey::Numeric(_))
    }

    /// Convert to a string representation for debugging
    pub fn to_debug_string(&self) -> String {
        match self {
            CacheKey::Numeric(n) => n.to_string(),
            CacheKey::String(s) => format!("\"{}\"", s),
            CacheKey::Bytes(bytes) => format!("bytes[{}]", bytes.len()),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cache_config_default() {
        let config = CacheConfig::default();
        assert_eq!(config.capacity, 1024 * 1024 * 1024);
        assert!(config.default_ttl.is_some());
        assert!(!config.consider_metadata);
        assert_eq!(config.hash_power, 20);
        assert!(config.validate().is_ok());
    }

    #[test]
    fn test_cache_config_validation() {
        // Test invalid capacity
        let result = CacheConfig::new(0, None, false, 20);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("capacity"));

        // Test invalid hash power
        let result = CacheConfig::new(1024, None, false, 5);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("Hash power"));

        let result = CacheConfig::new(1024, None, false, 35);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("Hash power"));

        // Test invalid TTL
        let result = CacheConfig::new(1024, Some(Duration::from_secs(0)), false, 20);
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("TTL"));

        // Test valid configuration
        let result = CacheConfig::new(1024, Some(Duration::from_secs(3600)), true, 15);
        assert!(result.is_ok());
    }

    #[test]
    fn test_cache_config_builder() {
        let config = CacheConfig::builder()
            .capacity_mb(512)
            .default_ttl_secs(7200)
            .consider_metadata(true)
            .hash_power(18)
            .build()
            .unwrap();

        assert_eq!(config.capacity, 512 * 1024 * 1024);
        assert_eq!(config.default_ttl.unwrap().as_secs(), 7200);
        assert!(config.consider_metadata);
        assert_eq!(config.hash_power, 18);
    }

    #[test]
    fn test_cache_config_builder_defaults() {
        let config = CacheConfig::builder().build().unwrap();
        assert_eq!(config.capacity, 1024 * 1024 * 1024);
        assert!(config.default_ttl.is_some());
        assert!(!config.consider_metadata);
        assert_eq!(config.hash_power, 20);
    }

    #[test]
    fn test_cache_config_conversions() {
        let config = CacheConfig {
            capacity: 2048,
            default_ttl: Some(Duration::from_secs(1800)),
            consider_metadata: true,
            hash_power: 16,
        };

        let c_params = config.to_common_cache_params();
        assert_eq!(c_params.cache_size, 2048);
        assert_eq!(c_params.default_ttl, 1800);
        assert_eq!(c_params.hashpower, 16);
        assert!(c_params.consider_obj_metadata);

        let config_back = CacheConfig::from_common_cache_params(&c_params);
        assert_eq!(config_back.capacity, config.capacity);
        assert_eq!(config_back.default_ttl.unwrap().as_secs(), 1800);
        assert_eq!(config_back.consider_metadata, config.consider_metadata);
        assert_eq!(config_back.hash_power, config.hash_power);
    }

    #[test]
    fn test_cache_key_conversions() {
        let key1: CacheKey = 42u64.into();
        assert_eq!(key1, CacheKey::Numeric(42));

        let key2: CacheKey = "test".into();
        assert_eq!(key2, CacheKey::String("test".to_string()));

        let key3: CacheKey = vec![1, 2, 3].into();
        assert_eq!(key3, CacheKey::Bytes(vec![1, 2, 3]));

        let key4: CacheKey = "hello".to_string().into();
        assert_eq!(key4, CacheKey::String("hello".to_string()));

        let key5: CacheKey = [1u8, 2, 3, 4].as_slice().into();
        assert_eq!(key5, CacheKey::Bytes(vec![1, 2, 3, 4]));
    }

    #[test]
    fn test_cache_key_obj_id_conversion() {
        let numeric_key = CacheKey::Numeric(12345);
        assert_eq!(numeric_key.to_obj_id(), 12345);
        assert_eq!(CacheKey::from_obj_id(12345), numeric_key);

        let string_key = CacheKey::String("test".to_string());
        let obj_id = string_key.to_obj_id();
        // Hash should be consistent
        assert_eq!(string_key.to_obj_id(), obj_id);

        let bytes_key = CacheKey::Bytes(vec![1, 2, 3, 4]);
        let obj_id2 = bytes_key.to_obj_id();
        // Hash should be consistent
        assert_eq!(bytes_key.to_obj_id(), obj_id2);
    }

    #[test]
    fn test_cache_key_properties() {
        let numeric_key = CacheKey::Numeric(42);
        assert!(numeric_key.is_numeric());
        assert_eq!(numeric_key.size_bytes(), 8);
        assert_eq!(numeric_key.to_debug_string(), "42");

        let string_key = CacheKey::String("hello".to_string());
        assert!(!string_key.is_numeric());
        assert_eq!(string_key.size_bytes(), 5);
        assert_eq!(string_key.to_debug_string(), "\"hello\"");

        let bytes_key = CacheKey::Bytes(vec![1, 2, 3]);
        assert!(!bytes_key.is_numeric());
        assert_eq!(bytes_key.size_bytes(), 3);
        assert_eq!(bytes_key.to_debug_string(), "bytes[3]");
    }

    #[test]
    fn test_cache_key_hash_consistency() {
        use std::collections::HashMap;

        let key1 = CacheKey::String("test".to_string());
        let key2 = CacheKey::String("test".to_string());
        let key3 = CacheKey::String("different".to_string());

        let mut map = HashMap::new();
        map.insert(key1.clone(), "value1");
        map.insert(key3.clone(), "value3");

        // Same content should be found
        assert_eq!(map.get(&key2), Some(&"value1"));
        assert_eq!(map.get(&key1), Some(&"value1"));
        assert_eq!(map.get(&key3), Some(&"value3"));
    }
}
