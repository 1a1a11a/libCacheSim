//! FFI (Foreign Function Interface) utilities and safe wrappers

pub mod bindings;
pub mod wrappers;

// Re-export the sys crate for internal use
pub use libcachesim_sys as sys;

/// Utility functions for FFI operations
pub mod utils {
    use crate::error::{CacheError, TraceError};
    use crate::ffi::sys;
    use std::ffi::{CStr, CString};
    use std::ptr;

    /// Convert a Rust string to a C string
    pub fn rust_string_to_c(s: &str) -> Result<CString, CacheError> {
        CString::new(s).map_err(|_| CacheError::invalid_key("String contains null bytes"))
    }

    /// Convert a C string to a Rust string
    ///
    /// # Safety
    ///
    /// The caller must ensure that `ptr` is a valid null-terminated C string
    /// and that it remains valid for the duration of the returned string's lifetime.
    pub unsafe fn c_string_to_rust(ptr: *const std::os::raw::c_char) -> Result<String, TraceError> {
        if ptr.is_null() {
            return Err(TraceError::ffi_error("Null pointer passed to c_string_to_rust"));
        }

        let c_str = unsafe { CStr::from_ptr(ptr) };
        c_str
            .to_str()
            .map(|s| s.to_string())
            .map_err(|_| TraceError::ffi_error("Invalid UTF-8 in C string"))
    }

    /// Check if a pointer is null and return an appropriate error
    pub fn check_null_ptr<T>(ptr: *mut T, context: &str) -> Result<*mut T, CacheError> {
        if ptr.is_null() {
            Err(CacheError::ffi_error(format!("Null pointer in {}", context)))
        } else {
            Ok(ptr)
        }
    }

    /// Convert a C boolean (int) to Rust bool
    pub fn c_bool_to_rust(value: std::os::raw::c_int) -> bool {
        value != 0
    }

    /// Convert a Rust bool to C boolean (int)
    pub fn rust_bool_to_c(value: bool) -> std::os::raw::c_int {
        if value { 1 } else { 0 }
    }

    /// Validate cache parameters
    pub fn validate_cache_params(capacity: u64) -> Result<(), CacheError> {
        if capacity == 0 {
            return Err(CacheError::invalid_configuration("Cache capacity cannot be zero"));
        }
        if capacity > u64::MAX / 2 {
            return Err(CacheError::invalid_configuration("Cache capacity too large"));
        }
        Ok(())
    }

    /// Create default cache parameters with validation
    pub fn create_default_cache_params(capacity: u64) -> Result<sys::common_cache_params_t, CacheError> {
        validate_cache_params(capacity)?;

        // Create default parameters manually since the function might not be available
        let params = sys::common_cache_params_t {
            cache_size: capacity,
            default_ttl: 364 * 86400, // 1 year in seconds
            hashpower: 20,
            consider_obj_metadata: false,
        };
        Ok(params)
    }

    /// Validate trace type
    pub fn validate_trace_type(trace_type: sys::trace_type_e) -> Result<(), TraceError> {
        // Check if trace type is within valid range
        // UNKNOWN_TRACE should be the last valid enum value
        const MAX_TRACE_TYPE: u32 = 13; // Based on the enum definition
        if trace_type as u32 >= MAX_TRACE_TYPE {
            return Err(TraceError::unsupported_trace_type(format!("Invalid trace type: {}", trace_type as u32)));
        }
        Ok(())
    }

    /// Create a request from basic parameters
    pub fn create_request(obj_id: u64, obj_size: i64, op: sys::req_op_e) -> Result<sys::request_t, CacheError> {
        if obj_size <= 0 {
            return Err(CacheError::invalid_operation("Object size must be positive"));
        }

        let mut req = unsafe { std::mem::zeroed::<sys::request_t>() };
        req.obj_id = obj_id;
        req.obj_size = obj_size;
        req.op = op;
        req.valid = true;
        req.next_access_vtime = -2;
        req.clock_time = 0;
        req.hv = 0;
        req.ttl = 0;

        Ok(req)
    }

    /// Validate request parameters
    pub fn validate_request(req: &sys::request_t) -> Result<(), CacheError> {
        if !req.valid {
            return Err(CacheError::invalid_operation("Request is not valid"));
        }
        if req.obj_size <= 0 {
            return Err(CacheError::invalid_operation("Object size must be positive"));
        }
        Ok(())
    }

    /// Memory management helper for C structures
    pub struct CStructGuard<T> {
        ptr: *mut T,
        free_fn: Option<unsafe fn(*mut T)>,
    }

    impl<T> CStructGuard<T> {
        /// Create a new guard for a C structure pointer
        ///
        /// # Safety
        ///
        /// The caller must ensure that `ptr` is a valid pointer to a C structure
        /// and that `free_fn` is the appropriate function to free it.
        pub unsafe fn new(ptr: *mut T, free_fn: Option<unsafe fn(*mut T)>) -> Self {
            Self { ptr, free_fn }
        }

        /// Get the raw pointer (for use in C function calls)
        pub fn as_ptr(&self) -> *mut T {
            self.ptr
        }

        /// Check if the pointer is null
        pub fn is_null(&self) -> bool {
            self.ptr.is_null()
        }

        /// Release the pointer without freeing (for transferring ownership)
        pub fn release(mut self) -> *mut T {
            let ptr = self.ptr;
            self.ptr = ptr::null_mut();
            ptr
        }
    }

    impl<T> Drop for CStructGuard<T> {
        fn drop(&mut self) {
            if !self.ptr.is_null() {
                if let Some(free_fn) = self.free_fn {
                    unsafe { free_fn(self.ptr) };
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use super::utils::*;

    #[test]
    fn test_string_conversions() {
        let rust_str = "hello world";
        let c_string = rust_string_to_c(rust_str).unwrap();

        unsafe {
            let back_to_rust = c_string_to_rust(c_string.as_ptr()).unwrap();
            assert_eq!(back_to_rust, rust_str);
        }
    }

    #[test]
    fn test_null_string_error() {
        let result = rust_string_to_c("hello\0world");
        assert!(result.is_err());
    }

    #[test]
    fn test_bool_conversions() {
        assert!(c_bool_to_rust(1));
        assert!(!c_bool_to_rust(0));
        assert!(c_bool_to_rust(-1)); // Non-zero is true in C

        assert_eq!(rust_bool_to_c(true), 1);
        assert_eq!(rust_bool_to_c(false), 0);
    }

    #[test]
    fn test_null_ptr_check() {
        let valid_ptr = &mut 42 as *mut i32;
        let null_ptr = std::ptr::null_mut::<i32>();

        assert!(check_null_ptr(valid_ptr, "test").is_ok());
        assert!(check_null_ptr(null_ptr, "test").is_err());
    }
}
