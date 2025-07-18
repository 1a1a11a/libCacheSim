//! Safe wrappers around libCacheSim C functions
//!
//! This module provides safe Rust wrappers around the unsafe C FFI functions.
//! All functions here should handle error checking and memory safety.

use crate::error::{CacheResult, TraceResult, error_utils};
use crate::ffi::{sys};
use crate::ffi::utils;

/// Safe wrapper around cache creation
///
/// Creates a cache using libCacheSim's cache_struct_init function.
pub fn create_cache_safe(
    algorithm: &str,
    params: &sys::common_cache_params_t,
) -> CacheResult<*mut sys::cache_t> {
    // Validate inputs
    if algorithm.is_empty() {
        return Err(crate::error::CacheError::invalid_parameter("algorithm", "create_cache_safe"));
    }

    // Convert algorithm name to C string
    let c_algorithm = utils::rust_string_to_c(algorithm)?;

    // Call the C function
    let cache_ptr = unsafe {
        // Create a copy of the params struct to pass by value
        let params_copy = sys::common_cache_params_t {
            cache_size: params.cache_size,
            default_ttl: params.default_ttl,
            hashpower: params.hashpower,
            consider_obj_metadata: params.consider_obj_metadata,
        };

        sys::cache_struct_init(
            c_algorithm.as_ptr(),
            params_copy,
            std::ptr::null_mut(), // No additional parameters
        )
    };

    // Check for null pointer
    error_utils::check_null_ptr_cache(cache_ptr, "cache_struct_init")
}

/// Safe wrapper around cache destruction
///
/// # Safety
///
/// The caller must ensure that `cache` is a valid cache pointer that was
/// created by `create_cache_safe` and has not been freed yet.
pub unsafe fn free_cache_safe(cache: *mut sys::cache_t) -> CacheResult<()> {
    // Check for null pointer
    error_utils::check_null_ptr_cache(cache, "free_cache_safe")?;

    // Free the cache
    unsafe { sys::cache_struct_free(cache) };
    Ok(())
}

/// Safe wrapper around cache get operation
///
/// # Safety
///
/// The caller must ensure that `cache` is a valid cache pointer.
pub unsafe fn cache_get_safe(
    cache: *mut sys::cache_t,
    request: &sys::request_t,
) -> CacheResult<bool> {
    // Check for null pointer
    error_utils::check_null_ptr_cache(cache, "cache_get_safe")?;

    // Validate request
    crate::ffi::bindings::validation::validate_request(request)?;

    // Call the cache get function
    let result = unsafe { sys::cache_get_base(cache, request) };

    Ok(result)
}

/// Safe wrapper around cache insert operation
///
/// # Safety
///
/// The caller must ensure that `cache` is a valid cache pointer.
pub unsafe fn cache_insert_safe(
    cache: *mut sys::cache_t,
    request: &sys::request_t,
) -> CacheResult<()> {
    // Check for null pointer
    error_utils::check_null_ptr_cache(cache, "cache_insert_safe")?;

    // Validate request
    crate::ffi::bindings::validation::validate_request(request)?;

    // Call the cache insert function
    let result = unsafe { sys::cache_insert_base(cache, request) };

    if result.is_null() {
        Err(crate::error::CacheError::cache_full(request.obj_size as u64))
    } else {
        Ok(())
    }
}

/// Safe wrapper around cache remove operation
///
/// # Safety
///
/// The caller must ensure that `cache` is a valid cache pointer.
pub unsafe fn cache_remove_safe(
    cache: *mut sys::cache_t,
    obj: *mut sys::cache_obj_t,
) -> CacheResult<()> {
    // Check for null pointer
    error_utils::check_null_ptr_cache(cache, "cache_remove_safe")?;
    error_utils::check_null_ptr_cache(obj, "cache_remove_safe obj")?;

    // Call the cache remove function
    unsafe { sys::cache_remove_obj_base(cache, obj, false) };

    Ok(())
}

/// Safe wrapper around trace reader creation
pub fn open_trace_safe(
    path: &str,
    trace_type: sys::trace_type_e,
    params: Option<&sys::reader_init_param_t>,
) -> TraceResult<*mut sys::reader_t> {
    // Validate inputs
    error_utils::validate_file_path(path)?;

    // Convert Rust string to C string
    let c_path = utils::rust_string_to_c(path)?;

    // Use provided params or default
    let default_params;
    let reader_params = match params {
        Some(p) => p,
        None => {
            default_params = unsafe { sys::libcachesim_default_reader_init_params() };
            &default_params
        }
    };

    // Call the C function
    let reader = unsafe { sys::setup_reader(c_path.as_ptr(), trace_type, reader_params) };

    // Check for null pointer using the new error utilities
    error_utils::check_null_ptr_trace(reader, "setup_reader")
}

/// Safe wrapper around trace reader destruction
///
/// # Safety
///
/// The caller must ensure that `reader` is a valid reader pointer that was
/// created by `open_trace_safe` and has not been freed yet.
pub unsafe fn close_trace_safe(reader: *mut sys::reader_t) -> TraceResult<()> {
    // Check for null pointer using the new error utilities
    error_utils::check_null_ptr_trace(reader, "close_reader")?;

    let result = unsafe { sys::close_reader(reader) };
    error_utils::check_c_result_trace(result, "close_reader")
}

/// Safe wrapper around reading one request from trace
///
/// # Safety
///
/// The caller must ensure that `reader` is a valid reader pointer.
pub unsafe fn read_one_req_safe(
    reader: *mut sys::reader_t,
) -> TraceResult<Option<sys::request_t>> {
    // Check for null pointer using the new error utilities
    error_utils::check_null_ptr_trace(reader, "read_one_req")?;

    // Allocate a new request on the stack (since new_request might not be available)
    let mut request = unsafe { std::mem::zeroed::<sys::request_t>() };

    // Read one request
    let result = unsafe { sys::read_one_req(reader, &mut request) };

    match result {
        0 => {
            // Success - return the request data
            Ok(Some(request))
        }
        1 => {
            // End of trace
            Ok(None)
        }
        _ => {
            // Convert error code to appropriate TraceError
            Err(error_utils::c_error_to_trace_error(result, "read_one_req"))
        }
    }
}

/// Safe wrapper around getting the number of requests in a trace
///
/// # Safety
///
/// The caller must ensure that `reader` is a valid reader pointer.
pub unsafe fn get_num_requests_safe(reader: *mut sys::reader_t) -> TraceResult<i64> {
    // Check for null pointer using the new error utilities
    error_utils::check_null_ptr_trace(reader, "get_num_of_req")?;

    let count = unsafe { sys::get_num_of_req(reader) };
    if count < 0 {
        Err(error_utils::c_error_to_trace_error(count as i32, "get_num_of_req"))
    } else {
        Ok(count)
    }
}

/// Safe wrapper around resetting a trace reader
///
/// # Safety
///
/// The caller must ensure that `reader` is a valid reader pointer.
pub unsafe fn reset_reader_safe(reader: *mut sys::reader_t) -> TraceResult<()> {
    // Check for null pointer using the new error utilities
    error_utils::check_null_ptr_trace(reader, "reset_reader")?;

    unsafe { sys::reset_reader(reader) };
    Ok(())
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_wrapper_functions_exist() {
        // Basic smoke test to ensure wrapper functions are defined
        // Actual functionality tests will be added when implementations are complete
    }

    #[test]
    fn test_ffi_types_exist() {
        // Test that the FFI types are accessible and have expected sizes
        use std::mem;

        // Test that basic types exist and have reasonable sizes
        assert!(mem::size_of::<sys::cache_t>() > 0);
        assert!(mem::size_of::<sys::reader_t>() > 0);
        assert!(mem::size_of::<sys::request_t>() > 0);
        assert!(mem::size_of::<sys::common_cache_params_t>() > 0);
        assert!(mem::size_of::<sys::reader_init_param_t>() > 0);

        // Test that enums are accessible
        let _trace_type = sys::trace_type_e::CSV_TRACE;
        let _req_op = sys::req_op_e::OP_GET;
    }

    #[test]
    fn test_request_creation() {
        // Test that we can create and manipulate request structures
        let mut req = unsafe { std::mem::zeroed::<sys::request_t>() };
        req.obj_id = 12345;
        req.obj_size = 1024;
        req.op = sys::req_op_e::OP_GET;

        assert_eq!(req.obj_id, 12345);
        assert_eq!(req.obj_size, 1024);
        assert_eq!(req.op, sys::req_op_e::OP_GET);
    }

    #[test]
    fn test_cache_params_creation() {
        // Test that we can create cache parameters
        let params = sys::common_cache_params_t {
            cache_size: 1024 * 1024,
            default_ttl: 3600,
            hashpower: 20,
            consider_obj_metadata: false,
        };

        assert_eq!(params.cache_size, 1024 * 1024);
        assert_eq!(params.default_ttl, 3600);
        assert_eq!(params.hashpower, 20);
        assert!(!params.consider_obj_metadata);
    }
}
