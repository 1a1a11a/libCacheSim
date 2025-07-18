//! Generated FFI bindings for libCacheSim
//!
//! This module re-exports the generated bindings from libcachesim-sys
//! and provides additional safety checks and validation.

// Re-export all the generated bindings
pub use libcachesim_sys::*;

/// Memory management helpers for request structures
pub mod request {
    use super::*;
    use crate::error::{CacheError, Result};

    /// Create a new request using the helper function
    pub fn new() -> Result<*mut request_t, CacheError> {
        let req = unsafe { libcachesim_new_request() };
        if req.is_null() {
            Err(CacheError::OutOfMemory)
        } else {
            Ok(req)
        }
    }

    /// Free a request using the helper function
    ///
    /// # Safety
    ///
    /// The caller must ensure that `req` is a valid request pointer
    /// that was created by `new()` and has not been freed yet.
    pub unsafe fn free(req: *mut request_t) {
        if !req.is_null() {
            unsafe { libcachesim_free_request(req) };
        }
    }

    /// Copy a request using the helper function
    ///
    /// # Safety
    ///
    /// The caller must ensure that both `dest` and `src` are valid request pointers.
    pub unsafe fn copy(dest: *mut request_t, src: *const request_t) -> Result<(), CacheError> {
        if dest.is_null() || src.is_null() {
            return Err(CacheError::ffi_error("Null pointer in request copy"));
        }
        unsafe { libcachesim_copy_request(dest, src) };
        Ok(())
    }

    /// Clone a request using the helper function
    ///
    /// # Safety
    ///
    /// The caller must ensure that `req` is a valid request pointer.
    pub unsafe fn clone(req: *const request_t) -> Result<*mut request_t, CacheError> {
        if req.is_null() {
            return Err(CacheError::ffi_error("Null pointer in request clone"));
        }
        let cloned = unsafe { libcachesim_clone_request(req) };
        if cloned.is_null() {
            Err(CacheError::OutOfMemory)
        } else {
            Ok(cloned)
        }
    }
}

/// Cache parameter helpers
pub mod cache_params {
    use super::*;

    /// Get default cache parameters using the helper function
    pub fn default() -> common_cache_params_t {
        unsafe { libcachesim_default_common_cache_params() }
    }

    /// Create cache parameters with custom capacity
    pub fn with_capacity(capacity: u64) -> common_cache_params_t {
        let mut params = default();
        params.cache_size = capacity;
        params
    }

    /// Create cache parameters with custom capacity and TTL
    pub fn with_capacity_and_ttl(capacity: u64, ttl: u64) -> common_cache_params_t {
        let mut params = default();
        params.cache_size = capacity;
        params.default_ttl = ttl;
        params
    }
}

/// Reader parameter helpers
pub mod reader_params {
    use super::*;

    /// Get default reader parameters using the helper function
    pub fn default() -> reader_init_param_t {
        unsafe { libcachesim_default_reader_init_params() }
    }

    /// Set default reader parameters in an existing struct
    ///
    /// # Safety
    ///
    /// The caller must ensure that `params` is a valid pointer to a reader_init_param_t.
    pub unsafe fn set_default(params: *mut reader_init_param_t) {
        if !params.is_null() {
            unsafe { libcachesim_set_default_reader_init_params(params) };
        }
    }

    /// Create reader parameters for CSV traces
    pub fn for_csv_trace() -> reader_init_param_t {
        let mut params = default();
        params.has_header = false;
        params.delimiter = b',' as i8;
        params.obj_id_field = 1;
        params.obj_size_field = 2;
        params.time_field = 0;
        params
    }

    /// Create reader parameters for CSV traces with header
    pub fn for_csv_trace_with_header() -> reader_init_param_t {
        let mut params = for_csv_trace();
        params.has_header = true;
        params.has_header_set = true;
        params
    }
}

/// Cache statistics helpers
pub mod cache_stats {
    use super::*;

    /// Get occupied bytes using the cache's function pointer or default
    ///
    /// # Safety
    ///
    /// The caller must ensure that `cache` is a valid cache pointer.
    pub unsafe fn get_occupied_bytes(cache: *const cache_t) -> i64 {
        if cache.is_null() {
            return 0;
        }

        let cache_ref = unsafe { &*cache };
        if let Some(get_occupied_byte_fn) = cache_ref.get_occupied_byte {
            unsafe { get_occupied_byte_fn(cache) }
        } else {
            // Fallback to default implementation
            unsafe { libcachesim_cache_get_occupied_byte_default(cache) }
        }
    }

    /// Get number of objects using the cache's function pointer or default
    ///
    /// # Safety
    ///
    /// The caller must ensure that `cache` is a valid cache pointer.
    pub unsafe fn get_n_objects(cache: *const cache_t) -> i64 {
        if cache.is_null() {
            return 0;
        }

        let cache_ref = unsafe { &*cache };
        if let Some(get_n_obj_fn) = cache_ref.get_n_obj {
            unsafe { get_n_obj_fn(cache) }
        } else {
            // Fallback to default implementation
            unsafe { libcachesim_cache_get_n_obj_default(cache) }
        }
    }

    /// Get total number of requests processed
    ///
    /// # Safety
    ///
    /// The caller must ensure that `cache` is a valid cache pointer.
    pub unsafe fn get_n_requests(cache: *const cache_t) -> i64 {
        if cache.is_null() {
            0
        } else {
            let cache_ref = unsafe { &*cache };
            cache_ref.n_req
        }
    }

    /// Get cache capacity
    ///
    /// # Safety
    ///
    /// The caller must ensure that `cache` is a valid cache pointer.
    pub unsafe fn get_cache_size(cache: *const cache_t) -> u64 {
        if cache.is_null() {
            0
        } else {
            let cache_ref = unsafe { &*cache };
            cache_ref.cache_size.max(0) as u64
        }
    }

    /// Get reference time using the helper function
    ///
    /// # Safety
    ///
    /// The caller must ensure that `cache` is a valid cache pointer.
    pub unsafe fn get_reference_time(cache: *const cache_t) -> i64 {
        if cache.is_null() {
            0
        } else {
            unsafe { libcachesim_cache_get_reference_time(cache) }
        }
    }
}

/// Validation helpers for FFI types
pub mod validation {
    use super::*;
    use crate::error::{CacheError, TraceError, Result};

    /// Validate a trace type enum value
    pub fn validate_trace_type(trace_type: trace_type_e) -> Result<(), TraceError> {
        // Check if trace type is within valid range
        match trace_type {
            trace_type_e::CSV_TRACE
            | trace_type_e::BIN_TRACE
            | trace_type_e::PLAIN_TXT_TRACE
            | trace_type_e::ORACLE_GENERAL_TRACE
            | trace_type_e::LCS_TRACE
            | trace_type_e::VSCSI_TRACE
            | trace_type_e::TWR_TRACE
            | trace_type_e::TWRNS_TRACE
            | trace_type_e::ORACLE_SIM_TWR_TRACE
            | trace_type_e::ORACLE_SYS_TWR_TRACE
            | trace_type_e::ORACLE_SIM_TWRNS_TRACE
            | trace_type_e::ORACLE_SYS_TWRNS_TRACE
            | trace_type_e::VALPIN_TRACE => Ok(()),
            trace_type_e::UNKNOWN_TRACE => {
                Err(TraceError::unsupported_trace_type("UNKNOWN_TRACE"))
            }
        }
    }

    /// Validate a request operation enum value
    pub fn validate_request_op(op: req_op_e) -> Result<(), CacheError> {
        match op {
            req_op_e::OP_NOP
            | req_op_e::OP_GET
            | req_op_e::OP_GETS
            | req_op_e::OP_SET
            | req_op_e::OP_ADD
            | req_op_e::OP_CAS
            | req_op_e::OP_REPLACE
            | req_op_e::OP_APPEND
            | req_op_e::OP_PREPEND
            | req_op_e::OP_DELETE
            | req_op_e::OP_INCR
            | req_op_e::OP_DECR
            | req_op_e::OP_READ
            | req_op_e::OP_WRITE
            | req_op_e::OP_UPDATE => Ok(()),
            req_op_e::OP_INVALID => Err(CacheError::invalid_operation("Invalid request operation")),
        }
    }

    /// Validate cache parameters
    pub fn validate_cache_params(params: &common_cache_params_t) -> Result<(), CacheError> {
        if params.cache_size == 0 {
            return Err(CacheError::invalid_configuration("Cache size cannot be zero"));
        }
        if params.cache_size > u64::MAX / 2 {
            return Err(CacheError::invalid_configuration("Cache size too large"));
        }
        if params.hashpower > 32 {
            return Err(CacheError::invalid_configuration("Hash power too large"));
        }
        Ok(())
    }

    /// Validate request parameters
    pub fn validate_request(req: &request_t) -> Result<(), CacheError> {
        if !req.valid {
            return Err(CacheError::invalid_operation("Request is not valid"));
        }
        if req.obj_size <= 0 {
            return Err(CacheError::invalid_operation("Object size must be positive"));
        }
        validate_request_op(req.op)?;
        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_request_helpers() {
        unsafe {
            let req = request::new().unwrap();
            assert!(!req.is_null());
            request::free(req);
        }
    }

    #[test]
    fn test_cache_params_helpers() {
        let params = cache_params::default();
        assert!(params.cache_size > 0);
        assert!(params.default_ttl > 0);

        let custom_params = cache_params::with_capacity(1024 * 1024);
        assert_eq!(custom_params.cache_size, 1024 * 1024);
    }

    #[test]
    fn test_reader_params_helpers() {
        let params = reader_params::default();
        assert_eq!(params.cap_at_n_req, -1);

        let csv_params = reader_params::for_csv_trace();
        assert_eq!(csv_params.delimiter, b',' as i8);
        assert!(!csv_params.has_header);

        let csv_header_params = reader_params::for_csv_trace_with_header();
        assert!(csv_header_params.has_header);
        assert!(csv_header_params.has_header_set);
    }

    #[test]
    fn test_validation_helpers() {
        // Test trace type validation
        assert!(validation::validate_trace_type(trace_type_e::CSV_TRACE).is_ok());
        assert!(validation::validate_trace_type(trace_type_e::UNKNOWN_TRACE).is_err());

        // Test request op validation
        assert!(validation::validate_request_op(req_op_e::OP_GET).is_ok());
        assert!(validation::validate_request_op(req_op_e::OP_INVALID).is_err());

        // Test cache params validation
        let valid_params = cache_params::default();
        assert!(validation::validate_cache_params(&valid_params).is_ok());

        let invalid_params = common_cache_params_t {
            cache_size: 0,
            default_ttl: 3600,
            hashpower: 20,
            consider_obj_metadata: false,
        };
        assert!(validation::validate_cache_params(&invalid_params).is_err());
    }
}
