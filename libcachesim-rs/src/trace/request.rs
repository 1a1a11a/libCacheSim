//! Cache request types and operations

use crate::cache::CacheKey;
use crate::ffi::bindings::{request_t, req_op_e};
use crate::error::{CacheError, Result};
use std::time::Duration;

/// A cache request representing a single operation in a trace
#[derive(Debug, Clone, PartialEq)]
pub struct CacheRequest {
    /// The cache key being accessed
    pub key: CacheKey,
    /// Size of the object in bytes
    pub size: u64,
    /// Type of operation being performed
    pub operation: Operation,
    /// Timestamp of the request (optional)
    pub timestamp: Option<u64>,
    /// Time-to-live for the object (optional)
    pub ttl: Option<Duration>,
}

impl CacheRequest {
    /// Create a new cache request
    pub fn new(key: CacheKey, size: u64, operation: Operation) -> Self {
        Self {
            key,
            size,
            operation,
            timestamp: None,
            ttl: None,
        }
    }

    /// Create a new GET request
    pub fn get(key: CacheKey) -> Self {
        Self::new(key, 0, Operation::Get)
    }

    /// Create a new SET request
    pub fn set(key: CacheKey, size: u64) -> Self {
        Self::new(key, size, Operation::Set)
    }

    /// Create a new DELETE request
    pub fn delete(key: CacheKey) -> Self {
        Self::new(key, 0, Operation::Delete)
    }

    /// Set the timestamp for this request
    pub fn with_timestamp(mut self, timestamp: u64) -> Self {
        self.timestamp = Some(timestamp);
        self
    }

    /// Set the TTL for this request
    pub fn with_ttl(mut self, ttl: Duration) -> Self {
        self.ttl = Some(ttl);
        self
    }

    /// Check if this is a read operation
    pub fn is_read(&self) -> bool {
        matches!(self.operation, Operation::Get | Operation::Read)
    }

    /// Check if this is a write operation
    pub fn is_write(&self) -> bool {
        matches!(self.operation, Operation::Set | Operation::Write | Operation::Update)
    }

    /// Check if this is a delete operation
    pub fn is_delete(&self) -> bool {
        matches!(self.operation, Operation::Delete)
    }
}

/// Cache operation types
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum Operation {
    /// Read/lookup operation (cache hit/miss check)
    Get,
    /// Write/insert operation
    Set,
    /// Delete/remove operation
    Delete,
    /// Generic read operation
    Read,
    /// Generic write operation
    Write,
    /// Update existing entry
    Update,
}

impl Operation {
    /// Get the string representation of the operation
    pub fn as_str(&self) -> &'static str {
        match self {
            Operation::Get => "get",
            Operation::Set => "set",
            Operation::Delete => "delete",
            Operation::Read => "read",
            Operation::Write => "write",
            Operation::Update => "update",
        }
    }

    /// Parse operation from string
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_lowercase().as_str() {
            "get" | "g" => Some(Operation::Get),
            "set" | "s" | "put" | "p" => Some(Operation::Set),
            "delete" | "del" | "d" | "remove" | "rm" => Some(Operation::Delete),
            "read" | "r" => Some(Operation::Read),
            "write" | "w" => Some(Operation::Write),
            "update" | "u" => Some(Operation::Update),
            _ => None,
        }
    }

    /// Check if this operation modifies the cache
    pub fn is_mutating(&self) -> bool {
        matches!(self, Operation::Set | Operation::Delete | Operation::Write | Operation::Update)
    }

    /// Check if this operation only reads from the cache
    pub fn is_read_only(&self) -> bool {
        matches!(self, Operation::Get | Operation::Read)
    }
}

impl std::fmt::Display for Operation {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

impl std::str::FromStr for Operation {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Operation::from_str(s).ok_or_else(|| format!("Unknown operation: {}", s))
    }
}

/// Conversion functions between Rust CacheRequest and C request_t
impl CacheRequest {
    /// Convert this Rust CacheRequest to a C request_t structure
    ///
    /// # Safety
    ///
    /// The returned pointer must be freed using `crate::ffi::bindings::request::free`
    /// when no longer needed to prevent memory leaks.
    pub fn to_c_request(&self) -> Result<*mut request_t> {
        use crate::ffi::bindings::request;

        let c_req = request::new()?;

        unsafe {
            // Set basic fields
            (*c_req).obj_size = self.size as i64;
            (*c_req).op = self.operation.to_c_op();
            (*c_req).valid = true;

            // Set object ID based on key type
            (*c_req).obj_id = self.key.to_obj_id();

            // Set timestamp if present
            if let Some(timestamp) = self.timestamp {
                (*c_req).clock_time = timestamp as i64;
            }

            // Set TTL if present
            if let Some(ttl) = self.ttl {
                (*c_req).ttl = ttl.as_secs() as i32;
            }

            // Initialize other fields to safe defaults
            (*c_req).hv = 0;
            (*c_req).tenant_id = 0;
            (*c_req).n_req = 0;
            (*c_req).next_access_vtime = -2;
            (*c_req).ns = 0;
            (*c_req).eviction_algo_data = std::ptr::null_mut();
            (*c_req).vtime_since_last_access = 0;
            (*c_req).rtime_since_last_access = 0;
            (*c_req).prev_size = 0;
            (*c_req).create_rtime = 0;
            (*c_req).compulsory_miss = false;
            (*c_req).overwrite = false;
            (*c_req).first_seen_in_window = false;
            (*c_req).n_features = 0;

            // Initialize kv struct - skip for now as it's causing issues
            // The kv struct is used for key-value cache traces
            // We'll handle this properly when implementing full trace support
        }

        Ok(c_req)
    }

    /// Create a Rust CacheRequest from a C request_t structure
    ///
    /// # Safety
    ///
    /// The caller must ensure that `c_req` is a valid pointer to a properly
    /// initialized request_t structure.
    pub unsafe fn from_c_request(c_req: *const request_t) -> Result<Self> {
        if c_req.is_null() {
            return Err(CacheError::ffi_error("Null request pointer"));
        }

        let req_ref = unsafe { &*c_req };

        if !req_ref.valid {
            return Err(CacheError::ffi_error("Invalid request"));
        }

        let key = CacheKey::from_obj_id(req_ref.obj_id);
        let size = req_ref.obj_size.max(0) as u64;
        let operation = Operation::from_c_op(req_ref.op)?;

        let mut rust_req = CacheRequest::new(key, size, operation);

        // Set timestamp if present
        if req_ref.clock_time > 0 {
            rust_req.timestamp = Some(req_ref.clock_time as u64);
        }

        // Set TTL if present
        if req_ref.ttl > 0 {
            rust_req.ttl = Some(Duration::from_secs(req_ref.ttl as u64));
        }

        Ok(rust_req)
    }
}

/// Conversion between Rust Operation and C req_op_e
impl Operation {
    /// Convert Rust Operation to C req_op_e
    pub fn to_c_op(&self) -> req_op_e {
        match self {
            Operation::Get => req_op_e::OP_GET,
            Operation::Set => req_op_e::OP_SET,
            Operation::Delete => req_op_e::OP_DELETE,
            Operation::Read => req_op_e::OP_READ,
            Operation::Write => req_op_e::OP_WRITE,
            Operation::Update => req_op_e::OP_UPDATE,
        }
    }

    /// Convert C req_op_e to Rust Operation
    pub fn from_c_op(c_op: req_op_e) -> Result<Self> {
        match c_op {
            req_op_e::OP_GET | req_op_e::OP_GETS => Ok(Operation::Get),
            req_op_e::OP_SET | req_op_e::OP_ADD | req_op_e::OP_REPLACE => Ok(Operation::Set),
            req_op_e::OP_DELETE => Ok(Operation::Delete),
            req_op_e::OP_READ => Ok(Operation::Read),
            req_op_e::OP_WRITE => Ok(Operation::Write),
            req_op_e::OP_UPDATE | req_op_e::OP_CAS => Ok(Operation::Update),
            req_op_e::OP_APPEND | req_op_e::OP_PREPEND => Ok(Operation::Update),
            req_op_e::OP_INCR | req_op_e::OP_DECR => Ok(Operation::Update),
            req_op_e::OP_NOP => Err(CacheError::invalid_operation("NOP operation not supported")),
            req_op_e::OP_INVALID => Err(CacheError::invalid_operation("Invalid operation")),
        }
    }
}



#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_cache_request_creation() {
        let req = CacheRequest::new(CacheKey::Numeric(42), 1024, Operation::Set);
        assert_eq!(req.key, CacheKey::Numeric(42));
        assert_eq!(req.size, 1024);
        assert_eq!(req.operation, Operation::Set);
        assert!(req.timestamp.is_none());
        assert!(req.ttl.is_none());
    }

    #[test]
    fn test_convenience_constructors() {
        let get_req = CacheRequest::get(CacheKey::Numeric(1));
        assert_eq!(get_req.operation, Operation::Get);
        assert_eq!(get_req.size, 0);

        let set_req = CacheRequest::set(CacheKey::String("key".to_string()), 512);
        assert_eq!(set_req.operation, Operation::Set);
        assert_eq!(set_req.size, 512);

        let del_req = CacheRequest::delete(CacheKey::Numeric(2));
        assert_eq!(del_req.operation, Operation::Delete);
        assert_eq!(del_req.size, 0);
    }

    #[test]
    fn test_request_with_metadata() {
        let req = CacheRequest::set(CacheKey::Numeric(1), 1024)
            .with_timestamp(12345)
            .with_ttl(Duration::from_secs(3600));

        assert_eq!(req.timestamp, Some(12345));
        assert_eq!(req.ttl, Some(Duration::from_secs(3600)));
    }

    #[test]
    fn test_operation_properties() {
        assert!(CacheRequest::get(CacheKey::Numeric(1)).is_read());
        assert!(CacheRequest::set(CacheKey::Numeric(1), 100).is_write());
        assert!(CacheRequest::delete(CacheKey::Numeric(1)).is_delete());
    }

    #[test]
    fn test_operation_string_conversion() {
        assert_eq!(Operation::Get.as_str(), "get");
        assert_eq!(Operation::Set.as_str(), "set");
        assert_eq!(Operation::Delete.as_str(), "delete");

        assert_eq!(Operation::from_str("get"), Some(Operation::Get));
        assert_eq!(Operation::from_str("SET"), Some(Operation::Set));
        assert_eq!(Operation::from_str("invalid"), None);
    }

    #[test]
    fn test_operation_mutating() {
        assert!(!Operation::Get.is_mutating());
        assert!(Operation::Set.is_mutating());
        assert!(Operation::Delete.is_mutating());

        assert!(Operation::Get.is_read_only());
        assert!(!Operation::Set.is_read_only());
    }

    #[test]
    fn test_operation_c_conversions() {
        use crate::ffi::bindings::req_op_e;

        // Test round-trip conversion for all operations
        let operations = [
            Operation::Get,
            Operation::Set,
            Operation::Delete,
            Operation::Read,
            Operation::Write,
            Operation::Update,
        ];

        for &op in &operations {
            let c_op = op.to_c_op();
            let back_to_rust = Operation::from_c_op(c_op).unwrap();
            assert_eq!(op, back_to_rust);
        }

        // Test specific C operation mappings
        assert_eq!(Operation::Get.to_c_op(), req_op_e::OP_GET);
        assert_eq!(Operation::Set.to_c_op(), req_op_e::OP_SET);
        assert_eq!(Operation::Delete.to_c_op(), req_op_e::OP_DELETE);

        // Test C to Rust conversions with multiple C ops mapping to same Rust op
        assert_eq!(Operation::from_c_op(req_op_e::OP_GET).unwrap(), Operation::Get);
        assert_eq!(Operation::from_c_op(req_op_e::OP_GETS).unwrap(), Operation::Get);
        assert_eq!(Operation::from_c_op(req_op_e::OP_SET).unwrap(), Operation::Set);
        assert_eq!(Operation::from_c_op(req_op_e::OP_ADD).unwrap(), Operation::Set);

        // Test error cases
        assert!(Operation::from_c_op(req_op_e::OP_NOP).is_err());
        assert!(Operation::from_c_op(req_op_e::OP_INVALID).is_err());
    }

    #[test]
    fn test_cache_key_obj_id_conversions() {
        // Test numeric key conversion
        let numeric_key = CacheKey::Numeric(12345);
        let obj_id = numeric_key.to_obj_id();
        assert_eq!(obj_id, 12345);

        let back_to_key = CacheKey::from_obj_id(obj_id);
        assert_eq!(back_to_key, CacheKey::Numeric(12345));

        // Test string key conversion (should produce consistent hash)
        let string_key = CacheKey::String("test_key".to_string());
        let obj_id1 = string_key.to_obj_id();
        let obj_id2 = string_key.to_obj_id();
        assert_eq!(obj_id1, obj_id2); // Should be consistent

        // Test bytes key conversion (should produce consistent hash)
        let bytes_key = CacheKey::Bytes(vec![1, 2, 3, 4]);
        let obj_id1 = bytes_key.to_obj_id();
        let obj_id2 = bytes_key.to_obj_id();
        assert_eq!(obj_id1, obj_id2); // Should be consistent
    }

    #[test]
    fn test_cache_request_c_conversions() {
        use std::time::Duration;

        // Create a test request
        let request = CacheRequest::new(
            CacheKey::Numeric(42),
            1024,
            Operation::Set,
        )
        .with_timestamp(12345)
        .with_ttl(Duration::from_secs(3600));

        // Convert to C request
        let c_req = request.to_c_request().unwrap();

        unsafe {
            // Verify C request fields
            assert_eq!((*c_req).obj_id, 42);
            assert_eq!((*c_req).obj_size, 1024);
            assert_eq!((*c_req).op, crate::ffi::bindings::req_op_e::OP_SET);
            assert_eq!((*c_req).clock_time, 12345);
            assert_eq!((*c_req).ttl, 3600);
            assert!((*c_req).valid);

            // Convert back to Rust request
            let back_to_rust = CacheRequest::from_c_request(c_req).unwrap();

            // Verify round-trip conversion
            assert_eq!(back_to_rust.key, CacheKey::Numeric(42));
            assert_eq!(back_to_rust.size, 1024);
            assert_eq!(back_to_rust.operation, Operation::Set);
            assert_eq!(back_to_rust.timestamp, Some(12345));
            assert_eq!(back_to_rust.ttl, Some(Duration::from_secs(3600)));

            // Clean up
            crate::ffi::bindings::request::free(c_req);
        }
    }

    #[test]
    fn test_cache_request_c_conversions_minimal() {
        // Test with minimal request (no timestamp, no TTL)
        let request = CacheRequest::get(CacheKey::String("test".to_string()));

        let c_req = request.to_c_request().unwrap();

        unsafe {
            // Verify basic fields
            assert_eq!((*c_req).obj_size, 0);
            assert_eq!((*c_req).op, crate::ffi::bindings::req_op_e::OP_GET);
            assert!((*c_req).valid);
            assert_eq!((*c_req).clock_time, 0);
            assert_eq!((*c_req).ttl, 0);

            // Convert back
            let back_to_rust = CacheRequest::from_c_request(c_req).unwrap();
            assert_eq!(back_to_rust.operation, Operation::Get);
            assert_eq!(back_to_rust.size, 0);
            assert!(back_to_rust.timestamp.is_none());
            assert!(back_to_rust.ttl.is_none());

            // Clean up
            crate::ffi::bindings::request::free(c_req);
        }
    }

    #[test]
    fn test_cache_request_c_conversions_error_cases() {
        // Test null pointer
        let result = unsafe { CacheRequest::from_c_request(std::ptr::null()) };
        assert!(result.is_err());

        // Test invalid request (would need to create an invalid C request to test this)
        // This is harder to test without creating invalid C structures
    }
}
