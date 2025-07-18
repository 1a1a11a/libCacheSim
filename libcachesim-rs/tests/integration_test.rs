//! Integration tests for libCacheSim Rust bindings
//!
//! These tests verify that the FFI bindings work correctly with the actual
//! libCacheSim C library.

use libcachesim::ffi::{bindings, sys};

#[test]
fn test_request_lifecycle() {
    // Test that we can create, manipulate, and free requests
    unsafe {
        let req = bindings::request::new().expect("Failed to create request");
        assert!(!req.is_null());

        // Verify the request has reasonable default values
        let req_data = &*req;
        assert!(req_data.valid);
        assert_eq!(req_data.obj_size, 1);
        assert_eq!(req_data.op, sys::req_op_e::OP_NOP);

        // Test cloning
        let cloned = bindings::request::clone(req).expect("Failed to clone request");
        assert!(!cloned.is_null());
        assert_ne!(req, cloned); // Different pointers

        // Verify cloned data matches
        let cloned_data = &*cloned;
        assert_eq!(req_data.valid, cloned_data.valid);
        assert_eq!(req_data.obj_size, cloned_data.obj_size);
        assert_eq!(req_data.op, cloned_data.op);

        // Clean up
        bindings::request::free(req);
        bindings::request::free(cloned);
    }
}

#[test]
fn test_cache_params_helpers() {
    // Test cache parameter creation and validation
    let default_params = bindings::cache_params::default();
    assert!(default_params.cache_size > 0);
    assert!(default_params.default_ttl > 0);
    assert!(default_params.hashpower > 0);

    // Test validation
    assert!(bindings::validation::validate_cache_params(&default_params).is_ok());

    // Test custom parameters
    let custom_params = bindings::cache_params::with_capacity(1024 * 1024);
    assert_eq!(custom_params.cache_size, 1024 * 1024);
    assert!(bindings::validation::validate_cache_params(&custom_params).is_ok());

    // Test invalid parameters
    let invalid_params = sys::common_cache_params_t {
        cache_size: 0,
        default_ttl: 3600,
        hashpower: 20,
        consider_obj_metadata: false,
    };
    assert!(bindings::validation::validate_cache_params(&invalid_params).is_err());
}

#[test]
fn test_reader_params_helpers() {
    // Test reader parameter creation
    let default_params = bindings::reader_params::default();
    assert_eq!(default_params.cap_at_n_req, -1);
    assert_eq!(default_params.delimiter, b',' as i8);

    // Test CSV-specific parameters
    let csv_params = bindings::reader_params::for_csv_trace();
    assert_eq!(csv_params.delimiter, b',' as i8);
    assert!(!csv_params.has_header);

    let csv_header_params = bindings::reader_params::for_csv_trace_with_header();
    assert!(csv_header_params.has_header);
    assert!(csv_header_params.has_header_set);
}

#[test]
fn test_enum_validation() {
    // Test trace type validation
    assert!(bindings::validation::validate_trace_type(sys::trace_type_e::CSV_TRACE).is_ok());
    assert!(bindings::validation::validate_trace_type(sys::trace_type_e::LCS_TRACE).is_ok());
    assert!(bindings::validation::validate_trace_type(sys::trace_type_e::UNKNOWN_TRACE).is_err());

    // Test request operation validation
    assert!(bindings::validation::validate_request_op(sys::req_op_e::OP_GET).is_ok());
    assert!(bindings::validation::validate_request_op(sys::req_op_e::OP_SET).is_ok());
    assert!(bindings::validation::validate_request_op(sys::req_op_e::OP_INVALID).is_err());
}

#[test]
fn test_request_validation() {
    unsafe {
        let req = bindings::request::new().expect("Failed to create request");

        // Create a copy of the request data for testing
        let mut req_data = sys::request_t {
            obj_size: 1024,
            op: sys::req_op_e::OP_GET,
            valid: true,
            ..std::mem::zeroed()
        };

        // Valid request should pass validation
        assert!(bindings::validation::validate_request(&req_data).is_ok());

        // Invalid size should fail
        req_data.obj_size = 0;
        assert!(bindings::validation::validate_request(&req_data).is_err());

        // Invalid operation should fail
        req_data.obj_size = 1024;
        req_data.op = sys::req_op_e::OP_INVALID;
        assert!(bindings::validation::validate_request(&req_data).is_err());

        // Invalid flag should fail
        req_data.op = sys::req_op_e::OP_GET;
        req_data.valid = false;
        assert!(bindings::validation::validate_request(&req_data).is_err());

        bindings::request::free(req);
    }
}

#[test]
fn test_ffi_type_sizes() {
    // Verify that important FFI types have reasonable sizes
    use std::mem;

    // These should be non-zero and reasonable
    assert!(mem::size_of::<sys::request_t>() > 0);
    assert!(mem::size_of::<sys::common_cache_params_t>() > 0);
    assert!(mem::size_of::<sys::reader_init_param_t>() > 0);

    // Enums should be small
    assert!(mem::size_of::<sys::trace_type_e>() <= 4);
    assert!(mem::size_of::<sys::req_op_e>() <= 4);

    // obj_id_t should be 8 bytes (u64)
    assert_eq!(mem::size_of::<sys::obj_id_t>(), 8);
}

#[test]
fn test_memory_safety() {
    // Test that we handle null pointers safely
    unsafe {
        // These should not crash
        bindings::request::free(std::ptr::null_mut());

        let result = bindings::request::copy(std::ptr::null_mut(), std::ptr::null());
        assert!(result.is_err());

        let result = bindings::request::clone(std::ptr::null());
        assert!(result.is_err());
    }
}

#[test]
fn test_constants_and_globals() {
    // Test that we can access global constants from the C library
    // Note: These might not be available depending on bindgen configuration
    // but we can test that the enum values are accessible

    let _csv_trace = sys::trace_type_e::CSV_TRACE;
    let _get_op = sys::req_op_e::OP_GET;
    let _set_op = sys::req_op_e::OP_SET;

    // Test that enum values have expected numeric values
    assert_eq!(sys::req_op_e::OP_NOP as u32, 0);
    assert_eq!(sys::req_op_e::OP_GET as u32, 1);
    assert_eq!(sys::req_op_e::OP_SET as u32, 3);
}
