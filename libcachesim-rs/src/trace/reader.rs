//! Trace file reader implementation
//!
//! This module provides the [`TraceReader`] type for reading and processing cache simulation
//! trace files. The implementation wraps the libCacheSim trace reader functionality with
//! safe Rust interfaces.
//!
//! ## Thread Safety
//!
//! The [`TraceReader`] type implements [`Send`] but not [`Sync`]:
//!
//! - **Safe to move between threads**: You can transfer ownership of a reader instance
//!   to another thread without issues.
//! - **Not safe for concurrent access**: Multiple threads cannot safely access the
//!   same reader instance simultaneously without external synchronization.
//!
//! ### Multi-threaded Usage
//!
//! For concurrent access, wrap the reader in a synchronization primitive:
//!
//! ```rust,no_run
//! use libcachesim::{TraceReader, TraceType, TraceConfig};
//! use std::sync::{Arc, Mutex};
//!
//! # let temp_file = std::env::temp_dir().join("test.csv");
//! # std::fs::write(&temp_file, "timestamp,obj_id,obj_size\n1,100,1024\n").unwrap();
//! let reader = Arc::new(Mutex::new(
//!     TraceReader::open(&temp_file, TraceType::Csv, TraceConfig::default())?
//! ));
//!
//! // Now multiple threads can safely access the reader
//! let reader_clone = Arc::clone(&reader);
//! std::thread::spawn(move || {
//!     let mut guard = reader_clone.lock().unwrap();
//!     if let Some(request) = guard.read_request().unwrap() {
//!         println!("Read request: {:?}", request);
//!     }
//! });
//! # std::fs::remove_file(temp_file).ok();
//! # Ok::<(), Box<dyn std::error::Error>>(())
//! ```
//!
//! ## Safety Guarantees
//!
//! - All operations are memory-safe and panic-safe
//! - Resources are automatically cleaned up via RAII
//! - No undefined behavior even if I/O operations fail
//! - Safe to drop from any thread
//! - Iterator implementation is safe for single-threaded use

use crate::error::{TraceError, Result};
use crate::trace::{CacheRequest, TraceConfig, TraceType};
use crate::trace::config::ObjectIdType;
use crate::ffi::wrappers;
use std::marker::PhantomData;
use std::path::Path;
use std::ptr::NonNull;

/// Trace file reader for processing cache simulation traces
///
/// This struct provides a safe wrapper around libCacheSim's trace reader functionality.
/// It supports various trace formats and provides both individual request reading
/// and iterator-based processing.
///
/// # Examples
///
/// ```rust,no_run
/// use libcachesim::{TraceReader, TraceType, TraceConfig};
///
/// // Open a CSV trace file
/// let mut reader = TraceReader::open(
///     "data/trace.csv",
///     TraceType::Csv,
///     TraceConfig::default()
/// )?;
///
/// // Read requests one by one
/// while let Some(request) = reader.read_request()? {
///     println!("Request: {:?}", request);
/// }
///
/// // Or use iterator interface
/// let mut reader = TraceReader::open(
///     "data/trace.csv",
///     TraceType::Csv,
///     TraceConfig::default()
/// )?;
///
/// for request_result in reader {
///     let request = request_result?;
///     println!("Request: {:?}", request);
/// }
/// # Ok::<(), libcachesim::TraceError>(())
/// ```
#[derive(Debug)]
pub struct TraceReader {
    inner: NonNull<libcachesim_sys::reader_t>,
    _phantom: PhantomData<libcachesim_sys::reader_t>,
}

impl TraceReader {
    /// Open a trace file for reading
    ///
    /// # Arguments
    ///
    /// * `path` - Path to the trace file
    /// * `trace_type` - Format of the trace file
    /// * `config` - Configuration for trace processing
    ///
    /// # Errors
    ///
    /// Returns `TraceError` if:
    /// - The file cannot be opened (file not found, permission denied, etc.)
    /// - The trace format is invalid or unsupported
    /// - The configuration is invalid
    /// - Memory allocation fails
    ///
    /// # Examples
    ///
    /// ```rust,no_run
    /// use libcachesim::{TraceReader, TraceType, TraceConfig};
    ///
    /// // Open a CSV trace with default configuration
    /// let reader = TraceReader::open(
    ///     "data/trace.csv",
    ///     TraceType::Csv,
    ///     TraceConfig::default()
    /// )?;
    ///
    /// // Open a binary trace with custom configuration
    /// let mut config = TraceConfig::default();
    /// config.ignore_size = true;
    /// config.default_size = 4096;
    ///
    /// let reader = TraceReader::open(
    ///     "data/trace.bin",
    ///     TraceType::Binary,
    ///     config
    /// )?;
    /// # Ok::<(), libcachesim::TraceError>(())
    /// ```
    pub fn open<P: AsRef<Path>>(
        path: P,
        trace_type: TraceType,
        config: TraceConfig,
    ) -> Result<Self, TraceError> {
        let path_str = path.as_ref().to_string_lossy();

        // Validate inputs
        if path_str.is_empty() {
            return Err(TraceError::invalid_configuration("Path cannot be empty"));
        }

        // Check if file exists
        if !path.as_ref().exists() {
            return Err(TraceError::file_system_error(format!(
                "Trace file does not exist: {}", path_str
            )));
        }

        // Convert trace type to C enum
        let c_trace_type = trace_type.to_c_trace_type();

        // Create reader parameters from config
        let reader_params = Self::config_to_reader_params(&config, trace_type)?;

        // Open the trace using the safe wrapper
        let reader_ptr = wrappers::open_trace_safe(&path_str, c_trace_type, Some(&reader_params))?;

        // Create NonNull wrapper
        let inner = NonNull::new(reader_ptr)
            .ok_or_else(|| TraceError::null_pointer("TraceReader::open"))?;

        Ok(TraceReader {
            inner,
            _phantom: PhantomData,
        })
    }

    /// Read the next request from the trace
    ///
    /// Returns `Ok(Some(request))` if a request was read successfully,
    /// `Ok(None)` if the end of the trace was reached, or an error if
    /// reading failed.
    ///
    /// # Errors
    ///
    /// Returns `TraceError` if:
    /// - The trace file is corrupted or has invalid format
    /// - An I/O error occurs while reading
    /// - The request data is invalid
    ///
    /// # Examples
    ///
    /// ```rust,no_run
    /// use libcachesim::{TraceReader, TraceType, TraceConfig};
    ///
    /// let mut reader = TraceReader::open(
    ///     "data/trace.csv",
    ///     TraceType::Csv,
    ///     TraceConfig::default()
    /// )?;
    ///
    /// while let Some(request) = reader.read_request()? {
    ///     println!("Key: {:?}, Size: {}, Op: {:?}",
    ///              request.key, request.size, request.operation);
    /// }
    /// # Ok::<(), libcachesim::TraceError>(())
    /// ```
    pub fn read_request(&mut self) -> Result<Option<CacheRequest>, TraceError> {
        // Use the safe wrapper to read one request
        let request_opt = unsafe { wrappers::read_one_req_safe(self.inner.as_ptr())? };

        match request_opt {
            Some(c_request) => {
                // Convert C request to Rust CacheRequest
                let rust_request = unsafe { CacheRequest::from_c_request(&c_request)? };
                Ok(Some(rust_request))
            }
            None => Ok(None), // End of trace
        }
    }

    /// Reset the reader to the beginning of the trace
    ///
    /// This allows re-reading the trace from the start without reopening the file.
    ///
    /// # Errors
    ///
    /// Returns `TraceError` if the reset operation fails (e.g., for non-seekable streams).
    ///
    /// # Examples
    ///
    /// ```rust,no_run
    /// use libcachesim::{TraceReader, TraceType, TraceConfig};
    ///
    /// let mut reader = TraceReader::open(
    ///     "data/trace.csv",
    ///     TraceType::Csv,
    ///     TraceConfig::default()
    /// )?;
    ///
    /// // Read some requests
    /// let _first_request = reader.read_request()?;
    /// let _second_request = reader.read_request()?;
    ///
    /// // Reset to beginning
    /// reader.reset()?;
    ///
    /// // Can now read from the beginning again
    /// let _first_request_again = reader.read_request()?;
    /// # Ok::<(), libcachesim::TraceError>(())
    /// ```
    pub fn reset(&mut self) -> Result<(), TraceError> {
        unsafe { wrappers::reset_reader_safe(self.inner.as_ptr()) }
    }

    /// Get the total number of requests in the trace (if known)
    ///
    /// Returns `Ok(Some(count))` if the total count is available,
    /// `Ok(None)` if the count cannot be determined (e.g., for streaming traces),
    /// or an error if the operation fails.
    ///
    /// Note: For some trace formats, determining the total count may require
    /// scanning the entire file, which can be expensive.
    ///
    /// # Examples
    ///
    /// ```rust,no_run
    /// use libcachesim::{TraceReader, TraceType, TraceConfig};
    ///
    /// let reader = TraceReader::open(
    ///     "data/trace.csv",
    ///     TraceType::Csv,
    ///     TraceConfig::default()
    /// )?;
    ///
    /// match reader.total_requests()? {
    ///     Some(count) => println!("Trace has {} requests", count),
    ///     None => println!("Request count unknown"),
    /// }
    /// # Ok::<(), libcachesim::TraceError>(())
    /// ```
    pub fn total_requests(&self) -> Result<Option<u64>, TraceError> {
        let count = unsafe { wrappers::get_num_requests_safe(self.inner.as_ptr())? };

        if count < 0 {
            // Negative count typically means unknown/unavailable
            Ok(None)
        } else {
            Ok(Some(count as u64))
        }
    }

    /// Convert TraceConfig to libCacheSim reader_init_param_t
    fn config_to_reader_params(
        config: &TraceConfig,
        trace_type: TraceType,
    ) -> Result<libcachesim_sys::reader_init_param_t, TraceError> {
        use crate::ffi::bindings::reader_params;

        // Start with appropriate defaults for the trace type
        let mut params = match trace_type {
            TraceType::Csv => reader_params::for_csv_trace_with_header(),
            _ => reader_params::default(),
        };

        // Apply configuration settings
        params.ignore_obj_size = config.ignore_size;
        params.ignore_size_zero_req = true; // Generally a good default

        // Set object ID type
        match config.obj_id_type {
            ObjectIdType::Numeric => {
                params.obj_id_is_num = true;
                params.obj_id_is_num_set = true;
            }
            ObjectIdType::String => {
                params.obj_id_is_num = false;
                params.obj_id_is_num_set = true;
            }
            ObjectIdType::Bytes => {
                params.obj_id_is_num = false;
                params.obj_id_is_num_set = true;
            }
        }

        // Set default object size if ignoring sizes
        if config.ignore_size {
            // Note: libCacheSim doesn't have a direct field for default size in reader params
            // The default size is typically handled at the request level
        }

        // Set TTL handling
        if config.consider_ttl {
            // TTL field configuration would go here if supported by the trace format
            // For now, we'll handle TTL at the request processing level
        }

        // Configure CSV-specific settings
        if trace_type == TraceType::Csv {
            params.has_header = true;
            params.has_header_set = true;
            params.delimiter = b',' as i8;

            // Set field positions for CSV (these are typical defaults)
            params.time_field = 0;      // Timestamp in first column
            params.obj_id_field = 1;    // Object ID in second column
            params.obj_size_field = 2;  // Object size in third column
            params.op_field = -1;       // Operation field not specified by default
        }

        Ok(params)
    }
}

impl Iterator for TraceReader {
    type Item = Result<CacheRequest, TraceError>;

    /// Get the next request from the trace
    ///
    /// This provides a convenient iterator interface over the trace requests.
    /// The iterator will yield `Ok(request)` for each successful request read,
    /// `Err(error)` if an error occurs, and `None` when the end of trace is reached.
    ///
    /// # Examples
    ///
    /// ```rust,no_run
    /// use libcachesim::{TraceReader, TraceType, TraceConfig};
    ///
    /// let reader = TraceReader::open(
    ///     "data/trace.csv",
    ///     TraceType::Csv,
    ///     TraceConfig::default()
    /// )?;
    ///
    /// for request_result in reader {
    ///     match request_result {
    ///         Ok(request) => {
    ///             println!("Processing request: {:?}", request);
    ///         }
    ///         Err(e) => {
    ///             eprintln!("Error reading request: {}", e);
    ///             break;
    ///         }
    ///     }
    /// }
    /// # Ok::<(), libcachesim::TraceError>(())
    /// ```
    fn next(&mut self) -> Option<Self::Item> {
        match self.read_request() {
            Ok(Some(req)) => Some(Ok(req)),
            Ok(None) => None,
            Err(e) => Some(Err(e)),
        }
    }
}

// Thread Safety Implementation
//
// Safety: TraceReader can be sent between threads but requires external synchronization for access.
//
// The underlying libCacheSim reader_t structure is not thread-safe. However, it is safe to
// transfer ownership of a TraceReader instance between threads (Send), but concurrent access
// from multiple threads requires external synchronization (not Sync).
//
// Rationale:
// - The C library reader maintains internal file state and buffers
// - File I/O operations are not thread-safe in the C implementation
// - Memory management is handled by Rust's RAII, making transfer safe
// - Users must use Mutex, RwLock, or similar for concurrent access
//
// Thread Safety Guarantees:
// - ✓ Safe to move between threads (Send)
// - ✗ Not safe for concurrent access without synchronization (not Sync)
// - ✓ Drop is safe from any thread
// - ✓ All methods are panic-safe and won't leave C structures in invalid state
// - ✓ Iterator implementation is safe for single-threaded use
unsafe impl Send for TraceReader {}

// Note: We do NOT implement Sync for TraceReader because:
// - The underlying C reader operations are not thread-safe
// - File I/O state would be corrupted by concurrent access
// - Users must use external synchronization (Mutex, RwLock, etc.) for shared access

impl Drop for TraceReader {
    /// Automatically close the trace reader when it goes out of scope
    ///
    /// This ensures that the underlying C resources are properly cleaned up.
    fn drop(&mut self) {
        // Close the reader using the safe wrapper
        // We ignore errors during cleanup since there's not much we can do about them
        let _ = unsafe { wrappers::close_trace_safe(self.inner.as_ptr()) };
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::trace::{TraceType, TraceConfig};
    use crate::trace::config::ObjectIdType;

    #[test]
    fn test_trace_reader_types() {
        // Basic smoke test for type definitions
        // Verify that TraceReader implements the expected traits
        fn assert_send<T: Send>() {}
        fn assert_iterator<T: Iterator>() {}

        assert_send::<TraceReader>();
        assert_iterator::<TraceReader>();
    }

    #[test]
    fn test_config_to_reader_params() {
        let config = TraceConfig {
            obj_id_type: ObjectIdType::Numeric,
            ignore_size: true,
            default_size: 4096,
            consider_ttl: false,
            default_ttl: 3600,
        };

        let params = TraceReader::config_to_reader_params(&config, TraceType::Csv).unwrap();

        assert!(params.ignore_obj_size);
        assert!(params.obj_id_is_num);
        assert!(params.obj_id_is_num_set);
        assert!(params.has_header);
        assert_eq!(params.delimiter, b',' as i8);
    }

    #[test]
    fn test_config_to_reader_params_string_ids() {
        let config = TraceConfig {
            obj_id_type: ObjectIdType::String,
            ignore_size: false,
            default_size: 1,
            consider_ttl: true,
            default_ttl: 7200,
        };

        let params = TraceReader::config_to_reader_params(&config, TraceType::Binary).unwrap();

        assert!(!params.ignore_obj_size);
        assert!(!params.obj_id_is_num);
        assert!(params.obj_id_is_num_set);
    }

    #[test]
    fn test_open_nonexistent_file() {
        let result = TraceReader::open(
            "/nonexistent/path/trace.csv",
            TraceType::Csv,
            TraceConfig::default(),
        );

        assert!(result.is_err());
        let error = result.unwrap_err();
        assert!(matches!(error, TraceError::FileSystemError { .. }));
        assert!(error.to_string().contains("does not exist"));
    }

    #[test]
    fn test_open_empty_path() {
        let result = TraceReader::open(
            "",
            TraceType::Csv,
            TraceConfig::default(),
        );

        assert!(result.is_err());
        let error = result.unwrap_err();
        assert!(matches!(error, TraceError::InvalidConfiguration { .. }));
        assert!(error.to_string().contains("empty"));
    }

    // Note: Integration tests with actual trace files will be added in task 11
    // For now, we focus on unit tests that don't require external files
}
