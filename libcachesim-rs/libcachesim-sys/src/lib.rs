#![allow(non_upper_case_globals)]
#![allow(non_camel_case_types)]
#![allow(non_snake_case)]
#![allow(improper_ctypes)]
#![allow(clippy::all)]

//! Low-level FFI bindings for libCacheSim
//!
//! This crate provides unsafe, low-level bindings to the libCacheSim C library.
//! For safe, idiomatic Rust APIs, use the `libcachesim` crate instead.

include!(concat!(env!("OUT_DIR"), "/bindings.rs"));

#[cfg(test)]
mod tests {
    use super::*;
    use std::ptr;

    #[test]
    fn test_request_creation() {
        unsafe {
            let req = libcachesim_new_request();
            assert!(!req.is_null());
            libcachesim_free_request(req);
        }
    }

    #[test]
    fn test_default_cache_params() {
        unsafe {
            let params = libcachesim_default_common_cache_params();
            assert!(params.cache_size > 0);
            assert!(params.default_ttl > 0);
        }
    }

    #[test]
    fn test_default_reader_params() {
        unsafe {
            let params = libcachesim_default_reader_init_params();
            assert_eq!(params.cap_at_n_req, -1);
            assert_eq!(params.delimiter, b',' as i8);
        }
    }
}
