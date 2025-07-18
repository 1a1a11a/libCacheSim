//! Error types and handling for libCacheSim Rust bindings

use thiserror::Error;
use std::ffi::NulError;

/// Result type alias for cache operations
pub type CacheResult<T> = std::result::Result<T, CacheError>;

/// Result type alias for trace operations
pub type TraceResult<T> = std::result::Result<T, TraceError>;

/// Generic result type that can handle both cache and trace errors
pub type Result<T, E = CacheError> = std::result::Result<T, E>;

/// Errors that can occur during cache operations
#[derive(Debug, Error)]
pub enum CacheError {
    /// Cache initialization failed
    #[error("Cache initialization failed: {message}")]
    InitializationFailed {
        /// Error message describing the initialization failure
        message: String,
    },

    /// Invalid cache operation
    #[error("Invalid cache operation: {message}")]
    InvalidOperation {
        /// Error message describing the invalid operation
        message: String,
    },

    /// Memory allocation failed
    #[error("Memory allocation failed")]
    OutOfMemory,

    /// Cache is full and cannot insert object
    #[error("Cache is full and cannot insert object of size {size} bytes")]
    CacheFull {
        /// Size of the object that couldn't be inserted
        size: u64,
    },

    /// Invalid cache configuration
    #[error("Invalid cache configuration: {message}")]
    InvalidConfiguration {
        /// Error message describing the configuration issue
        message: String,
    },

    /// Cache algorithm not supported
    #[error("Cache algorithm '{algorithm}' is not supported")]
    UnsupportedAlgorithm {
        /// Name of the unsupported algorithm
        algorithm: String,
    },

    /// Invalid key format
    #[error("Invalid key format: {message}")]
    InvalidKey {
        /// Error message describing the key format issue
        message: String,
    },

    /// FFI operation failed
    #[error("FFI operation failed: {message}")]
    FfiError {
        /// Error message from the FFI operation
        message: String,
    },

    /// Null pointer encountered
    #[error("Null pointer encountered in {context}")]
    NullPointer {
        /// Context where the null pointer was encountered
        context: String,
    },

    /// C library returned an error code
    #[error("C library error: code {code} in {context}")]
    CLibraryError {
        /// Error code returned by the C library
        code: i32,
        /// Context where the error occurred
        context: String,
    },

    /// Invalid parameter passed to C function
    #[error("Invalid parameter: {parameter} in {context}")]
    InvalidParameter {
        /// Name of the invalid parameter
        parameter: String,
        /// Context where the parameter was used
        context: String,
    },

    /// Resource limit exceeded
    #[error("Resource limit exceeded: {resource} in {context}")]
    ResourceLimitExceeded {
        /// Name of the resource that exceeded its limit
        resource: String,
        /// Context where the limit was exceeded
        context: String,
    },

    /// Generic cache error
    #[error("Cache error: {message}")]
    Other {
        /// Generic error message
        message: String,
    },
}

impl CacheError {
    /// Create a new initialization error
    pub fn initialization_failed<S: Into<String>>(message: S) -> Self {
        Self::InitializationFailed {
            message: message.into(),
        }
    }

    /// Create a new invalid operation error
    pub fn invalid_operation<S: Into<String>>(message: S) -> Self {
        Self::InvalidOperation {
            message: message.into(),
        }
    }

    /// Create a new cache full error
    pub fn cache_full(size: u64) -> Self {
        Self::CacheFull { size }
    }

    /// Create a new invalid configuration error
    pub fn invalid_configuration<S: Into<String>>(message: S) -> Self {
        Self::InvalidConfiguration {
            message: message.into(),
        }
    }

    /// Create a new unsupported algorithm error
    pub fn unsupported_algorithm<S: Into<String>>(algorithm: S) -> Self {
        Self::UnsupportedAlgorithm {
            algorithm: algorithm.into(),
        }
    }

    /// Create a new invalid key error
    pub fn invalid_key<S: Into<String>>(message: S) -> Self {
        Self::InvalidKey {
            message: message.into(),
        }
    }

    /// Create a new FFI error
    pub fn ffi_error<S: Into<String>>(message: S) -> Self {
        Self::FfiError {
            message: message.into(),
        }
    }

    /// Create a generic error
    pub fn other<S: Into<String>>(message: S) -> Self {
        Self::Other {
            message: message.into(),
        }
    }

    /// Create a null pointer error
    pub fn null_pointer<S: Into<String>>(context: S) -> Self {
        Self::NullPointer {
            context: context.into(),
        }
    }

    /// Create a C library error
    pub fn c_library_error<S: Into<String>>(code: i32, context: S) -> Self {
        Self::CLibraryError {
            code,
            context: context.into(),
        }
    }

    /// Create an invalid parameter error
    pub fn invalid_parameter<S: Into<String>, T: Into<String>>(parameter: S, context: T) -> Self {
        Self::InvalidParameter {
            parameter: parameter.into(),
            context: context.into(),
        }
    }

    /// Create a resource limit exceeded error
    pub fn resource_limit_exceeded<S: Into<String>, T: Into<String>>(resource: S, context: T) -> Self {
        Self::ResourceLimitExceeded {
            resource: resource.into(),
            context: context.into(),
        }
    }
}

/// Errors that can occur during trace processing
#[derive(Debug, Error)]
pub enum TraceError {
    /// Failed to open trace file
    #[error("Failed to open trace file '{path}': {source}")]
    FileOpenError {
        /// Path to the trace file that failed to open
        path: String,
        /// The underlying I/O error
        #[source]
        source: std::io::Error,
    },

    /// Invalid trace format
    #[error("Invalid trace format: {message}")]
    InvalidFormat {
        /// Error message describing the format issue
        message: String,
    },

    /// Trace parsing error
    #[error("Trace parsing error at line {line}: {message}")]
    ParseError {
        /// Line number where the parsing error occurred
        line: u64,
        /// Error message describing the parsing issue
        message: String,
    },

    /// End of trace reached
    #[error("End of trace reached")]
    EndOfTrace,

    /// Invalid trace configuration
    #[error("Invalid trace configuration: {message}")]
    InvalidConfiguration {
        /// Error message describing the configuration issue
        message: String,
    },

    /// Unsupported trace type
    #[error("Unsupported trace type: {trace_type}")]
    UnsupportedTraceType {
        /// Name of the unsupported trace type
        trace_type: String,
    },

    /// FFI operation failed
    #[error("FFI operation failed: {message}")]
    FfiError {
        /// Error message from the FFI operation
        message: String,
    },

    /// I/O error during trace processing
    #[error("I/O error: {source}")]
    IoError {
        /// The underlying I/O error
        #[from]
        source: std::io::Error,
    },

    /// Null pointer encountered
    #[error("Null pointer encountered in {context}")]
    NullPointer {
        /// Context where the null pointer was encountered
        context: String,
    },

    /// C library returned an error code
    #[error("C library error: code {code} in {context}")]
    CLibraryError {
        /// Error code returned by the C library
        code: i32,
        /// Context where the error occurred
        context: String,
    },

    /// Invalid parameter passed to C function
    #[error("Invalid parameter: {parameter} in {context}")]
    InvalidParameter {
        /// Name of the invalid parameter
        parameter: String,
        /// Context where the parameter was used
        context: String,
    },

    /// File system error
    #[error("File system error: {message}")]
    FileSystemError {
        /// Error message describing the file system issue
        message: String,
    },

    /// Generic trace error
    #[error("Trace error: {message}")]
    Other {
        /// Generic error message
        message: String,
    },
}

impl TraceError {
    /// Create a new file open error
    pub fn file_open_error<P: Into<String>>(path: P, source: std::io::Error) -> Self {
        Self::FileOpenError {
            path: path.into(),
            source,
        }
    }

    /// Create a new invalid format error
    pub fn invalid_format<S: Into<String>>(message: S) -> Self {
        Self::InvalidFormat {
            message: message.into(),
        }
    }

    /// Create a new parse error
    pub fn parse_error<S: Into<String>>(line: u64, message: S) -> Self {
        Self::ParseError {
            line,
            message: message.into(),
        }
    }

    /// Create a new invalid configuration error
    pub fn invalid_configuration<S: Into<String>>(message: S) -> Self {
        Self::InvalidConfiguration {
            message: message.into(),
        }
    }

    /// Create a new unsupported trace type error
    pub fn unsupported_trace_type<S: Into<String>>(trace_type: S) -> Self {
        Self::UnsupportedTraceType {
            trace_type: trace_type.into(),
        }
    }

    /// Create a new FFI error
    pub fn ffi_error<S: Into<String>>(message: S) -> Self {
        Self::FfiError {
            message: message.into(),
        }
    }

    /// Create a generic error
    pub fn other<S: Into<String>>(message: S) -> Self {
        Self::Other {
            message: message.into(),
        }
    }

    /// Create a null pointer error
    pub fn null_pointer<S: Into<String>>(context: S) -> Self {
        Self::NullPointer {
            context: context.into(),
        }
    }

    /// Create a C library error
    pub fn c_library_error<S: Into<String>>(code: i32, context: S) -> Self {
        Self::CLibraryError {
            code,
            context: context.into(),
        }
    }

    /// Create an invalid parameter error
    pub fn invalid_parameter<S: Into<String>, T: Into<String>>(parameter: S, context: T) -> Self {
        Self::InvalidParameter {
            parameter: parameter.into(),
            context: context.into(),
        }
    }

    /// Create a file system error
    pub fn file_system_error<S: Into<String>>(message: S) -> Self {
        Self::FileSystemError {
            message: message.into(),
        }
    }
}

// From trait implementations for converting C errors to Rust errors

/// Convert NulError (from CString creation) to CacheError
impl From<NulError> for CacheError {
    fn from(err: NulError) -> Self {
        Self::InvalidKey {
            message: format!("String contains null bytes: {}", err),
        }
    }
}

/// Convert NulError (from CString creation) to TraceError
impl From<NulError> for TraceError {
    fn from(err: NulError) -> Self {
        Self::InvalidConfiguration {
            message: format!("String contains null bytes: {}", err),
        }
    }
}

/// Convert std::io::Error to CacheError
impl From<std::io::Error> for CacheError {
    fn from(err: std::io::Error) -> Self {
        Self::FfiError {
            message: format!("I/O error: {}", err),
        }
    }
}

/// Convert CacheError to TraceError for cases where cache operations are used in trace processing
impl From<CacheError> for TraceError {
    fn from(err: CacheError) -> Self {
        match err {
            CacheError::OutOfMemory => Self::Other {
                message: "Out of memory during trace processing".to_string(),
            },
            CacheError::InvalidKey { message } => Self::InvalidFormat { message },
            CacheError::InvalidConfiguration { message } => Self::InvalidConfiguration { message },
            CacheError::FfiError { message } => Self::FfiError { message },
            CacheError::NullPointer { context } => Self::NullPointer { context },
            CacheError::CLibraryError { code, context } => Self::CLibraryError { code, context },
            CacheError::InvalidParameter { parameter, context } => Self::InvalidParameter { parameter, context },
            _ => Self::Other {
                message: format!("Cache error in trace processing: {}", err),
            },
        }
    }
}

/// Error handling utilities for common C library failure modes
pub mod error_utils {
    use super::*;

    /// Common C library error codes and their meanings
    #[derive(Debug, Clone, Copy, PartialEq, Eq)]
    pub enum CErrorCode {
        /// Success (0)
        Success = 0,
        /// Generic error (-1)
        GenericError = -1,
        /// Invalid argument (-2)
        InvalidArgument = -2,
        /// Out of memory (-3)
        OutOfMemory = -3,
        /// File not found (-4)
        FileNotFound = -4,
        /// Permission denied (-5)
        PermissionDenied = -5,
        /// End of file/trace (-6)
        EndOfFile = -6,
        /// Invalid format (-7)
        InvalidFormat = -7,
        /// Buffer too small (-8)
        BufferTooSmall = -8,
        /// Operation not supported (-9)
        NotSupported = -9,
        /// Resource busy (-10)
        ResourceBusy = -10,
    }

    impl CErrorCode {
        /// Convert an integer error code to a CErrorCode enum
        pub fn from_code(code: i32) -> Self {
            match code {
                0 => Self::Success,
                -1 => Self::GenericError,
                -2 => Self::InvalidArgument,
                -3 => Self::OutOfMemory,
                -4 => Self::FileNotFound,
                -5 => Self::PermissionDenied,
                -6 => Self::EndOfFile,
                -7 => Self::InvalidFormat,
                -8 => Self::BufferTooSmall,
                -9 => Self::NotSupported,
                -10 => Self::ResourceBusy,
                _ => Self::GenericError,
            }
        }

        /// Get a human-readable description of the error code
        pub fn description(&self) -> &'static str {
            match self {
                Self::Success => "Success",
                Self::GenericError => "Generic error",
                Self::InvalidArgument => "Invalid argument",
                Self::OutOfMemory => "Out of memory",
                Self::FileNotFound => "File not found",
                Self::PermissionDenied => "Permission denied",
                Self::EndOfFile => "End of file",
                Self::InvalidFormat => "Invalid format",
                Self::BufferTooSmall => "Buffer too small",
                Self::NotSupported => "Operation not supported",
                Self::ResourceBusy => "Resource busy",
            }
        }
    }

    /// Convert a C library error code to a CacheError
    pub fn c_error_to_cache_error(code: i32, context: &str) -> CacheError {
        let error_code = CErrorCode::from_code(code);
        match error_code {
            CErrorCode::Success => {
                // This shouldn't happen, but handle it gracefully
                CacheError::other(format!("Unexpected success code in error context: {}", context))
            }
            CErrorCode::OutOfMemory => CacheError::OutOfMemory,
            CErrorCode::InvalidArgument => {
                CacheError::invalid_parameter("unknown", context)
            }
            CErrorCode::NotSupported => {
                CacheError::invalid_operation(format!("Operation not supported: {}", context))
            }
            CErrorCode::ResourceBusy => {
                CacheError::resource_limit_exceeded("cache", context)
            }
            _ => CacheError::c_library_error(code, context),
        }
    }

    /// Convert a C library error code to a TraceError
    pub fn c_error_to_trace_error(code: i32, context: &str) -> TraceError {
        let error_code = CErrorCode::from_code(code);
        match error_code {
            CErrorCode::Success => {
                // This shouldn't happen, but handle it gracefully
                TraceError::other(format!("Unexpected success code in error context: {}", context))
            }
            CErrorCode::FileNotFound => {
                TraceError::file_system_error(format!("File not found: {}", context))
            }
            CErrorCode::PermissionDenied => {
                TraceError::file_system_error(format!("Permission denied: {}", context))
            }
            CErrorCode::EndOfFile => TraceError::EndOfTrace,
            CErrorCode::InvalidFormat => {
                TraceError::invalid_format(format!("Invalid format in {}", context))
            }
            CErrorCode::InvalidArgument => {
                TraceError::invalid_parameter("unknown", context)
            }
            CErrorCode::BufferTooSmall => {
                TraceError::other(format!("Buffer too small in {}", context))
            }
            _ => TraceError::c_library_error(code, context),
        }
    }

    /// Check if a C function returned an error and convert it to a Rust error
    pub fn check_c_result_cache(result: i32, context: &str) -> Result<(), CacheError> {
        if result == 0 {
            Ok(())
        } else {
            Err(c_error_to_cache_error(result, context))
        }
    }

    /// Check if a C function returned an error and convert it to a Rust trace error
    pub fn check_c_result_trace(result: i32, context: &str) -> Result<(), TraceError> {
        if result == 0 {
            Ok(())
        } else {
            Err(c_error_to_trace_error(result, context))
        }
    }

    /// Check if a pointer is null and return an appropriate error
    pub fn check_null_ptr_cache<T>(ptr: *mut T, context: &str) -> Result<*mut T, CacheError> {
        if ptr.is_null() {
            Err(CacheError::null_pointer(context))
        } else {
            Ok(ptr)
        }
    }

    /// Check if a pointer is null and return an appropriate trace error
    pub fn check_null_ptr_trace<T>(ptr: *mut T, context: &str) -> Result<*mut T, TraceError> {
        if ptr.is_null() {
            Err(TraceError::null_pointer(context))
        } else {
            Ok(ptr)
        }
    }

    /// Validate cache capacity and return appropriate error
    pub fn validate_cache_capacity(capacity: u64) -> Result<(), CacheError> {
        if capacity == 0 {
            Err(CacheError::invalid_configuration("Cache capacity cannot be zero"))
        } else if capacity > u64::MAX / 2 {
            Err(CacheError::invalid_configuration("Cache capacity too large"))
        } else {
            Ok(())
        }
    }

    /// Validate object size and return appropriate error
    pub fn validate_object_size(size: i64) -> Result<(), CacheError> {
        if size <= 0 {
            Err(CacheError::invalid_operation("Object size must be positive"))
        } else if size > i64::MAX / 2 {
            Err(CacheError::invalid_operation("Object size too large"))
        } else {
            Ok(())
        }
    }

    /// Validate file path and return appropriate error
    pub fn validate_file_path(path: &str) -> Result<(), TraceError> {
        if path.is_empty() {
            Err(TraceError::invalid_configuration("File path cannot be empty"))
        } else if path.contains('\0') {
            Err(TraceError::invalid_configuration("File path contains null bytes"))
        } else {
            Ok(())
        }
    }

    /// Convert a C string result to a Rust string with error handling
    ///
    /// # Safety
    ///
    /// The caller must ensure that `ptr` is either null or points to a valid
    /// null-terminated C string.
    pub unsafe fn c_string_to_rust_safe(ptr: *const std::os::raw::c_char) -> Result<Option<String>, TraceError> {
        if ptr.is_null() {
            Ok(None)
        } else {
            let c_str = unsafe { std::ffi::CStr::from_ptr(ptr) };
            match c_str.to_str() {
                Ok(s) => Ok(Some(s.to_string())),
                Err(_) => Err(TraceError::invalid_format("Invalid UTF-8 in C string")),
            }
        }
    }

    /// Handle common C library initialization patterns
    pub fn handle_c_init_result<T>(
        ptr: *mut T,
        context: &str,
        check_errno: bool,
    ) -> Result<*mut T, CacheError> {
        if ptr.is_null() {
            if check_errno {
                // In a real implementation, we might check errno here
                // For now, we'll assume it's an out of memory error
                Err(CacheError::OutOfMemory)
            } else {
                Err(CacheError::initialization_failed(format!("Failed to initialize {}", context)))
            }
        } else {
            Ok(ptr)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use super::error_utils::*;

    #[test]
    fn test_cache_error_creation() {
        let err = CacheError::initialization_failed("test message");
        assert!(matches!(err, CacheError::InitializationFailed { .. }));
        assert!(err.to_string().contains("test message"));

        let err = CacheError::cache_full(1024);
        assert!(matches!(err, CacheError::CacheFull { size: 1024 }));
        assert!(err.to_string().contains("1024"));

        let err = CacheError::null_pointer("test context");
        assert!(matches!(err, CacheError::NullPointer { .. }));
        assert!(err.to_string().contains("test context"));

        let err = CacheError::c_library_error(-1, "test function");
        assert!(matches!(err, CacheError::CLibraryError { code: -1, .. }));
        assert!(err.to_string().contains("-1"));
        assert!(err.to_string().contains("test function"));
    }

    #[test]
    fn test_trace_error_creation() {
        let io_err = std::io::Error::new(std::io::ErrorKind::NotFound, "file not found");
        let err = TraceError::file_open_error("test.csv", io_err);
        assert!(matches!(err, TraceError::FileOpenError { .. }));
        assert!(err.to_string().contains("test.csv"));

        let err = TraceError::parse_error(42, "invalid format");
        assert!(matches!(err, TraceError::ParseError { line: 42, .. }));
        assert!(err.to_string().contains("42"));

        let err = TraceError::null_pointer("reader creation");
        assert!(matches!(err, TraceError::NullPointer { .. }));
        assert!(err.to_string().contains("reader creation"));

        let err = TraceError::file_system_error("permission denied");
        assert!(matches!(err, TraceError::FileSystemError { .. }));
        assert!(err.to_string().contains("permission denied"));
    }

    #[test]
    fn test_error_display() {
        let cache_err = CacheError::OutOfMemory;
        assert_eq!(cache_err.to_string(), "Memory allocation failed");

        let trace_err = TraceError::EndOfTrace;
        assert_eq!(trace_err.to_string(), "End of trace reached");
    }

    #[test]
    fn test_from_traits() {
        // Test NulError conversion to CacheError
        let nul_err = std::ffi::CString::new("test\0string").unwrap_err();
        let cache_err: CacheError = nul_err.into();
        assert!(matches!(cache_err, CacheError::InvalidKey { .. }));

        // Test NulError conversion to TraceError
        let nul_err = std::ffi::CString::new("test\0string").unwrap_err();
        let trace_err: TraceError = nul_err.into();
        assert!(matches!(trace_err, TraceError::InvalidConfiguration { .. }));

        // Test std::io::Error conversion to CacheError
        let io_err = std::io::Error::new(std::io::ErrorKind::PermissionDenied, "access denied");
        let cache_err: CacheError = io_err.into();
        assert!(matches!(cache_err, CacheError::FfiError { .. }));
        assert!(cache_err.to_string().contains("access denied"));

        // Test CacheError to TraceError conversion
        let cache_err = CacheError::OutOfMemory;
        let trace_err: TraceError = cache_err.into();
        assert!(matches!(trace_err, TraceError::Other { .. }));
        assert!(trace_err.to_string().contains("Out of memory"));
    }

    #[test]
    fn test_c_error_code_conversion() {
        assert_eq!(CErrorCode::from_code(0), CErrorCode::Success);
        assert_eq!(CErrorCode::from_code(-1), CErrorCode::GenericError);
        assert_eq!(CErrorCode::from_code(-3), CErrorCode::OutOfMemory);
        assert_eq!(CErrorCode::from_code(-4), CErrorCode::FileNotFound);
        assert_eq!(CErrorCode::from_code(-999), CErrorCode::GenericError); // Unknown code

        assert_eq!(CErrorCode::Success.description(), "Success");
        assert_eq!(CErrorCode::OutOfMemory.description(), "Out of memory");
        assert_eq!(CErrorCode::FileNotFound.description(), "File not found");
    }

    #[test]
    fn test_c_error_to_cache_error() {
        let err = c_error_to_cache_error(-3, "cache_create");
        assert!(matches!(err, CacheError::OutOfMemory));

        let err = c_error_to_cache_error(-2, "cache_get");
        assert!(matches!(err, CacheError::InvalidParameter { .. }));

        let err = c_error_to_cache_error(-9, "cache_operation");
        assert!(matches!(err, CacheError::InvalidOperation { .. }));

        let err = c_error_to_cache_error(-10, "cache_insert");
        assert!(matches!(err, CacheError::ResourceLimitExceeded { .. }));

        let err = c_error_to_cache_error(-999, "unknown_function");
        assert!(matches!(err, CacheError::CLibraryError { code: -999, .. }));
    }

    #[test]
    fn test_c_error_to_trace_error() {
        let err = c_error_to_trace_error(-4, "trace_open");
        assert!(matches!(err, TraceError::FileSystemError { .. }));
        assert!(err.to_string().contains("File not found"));

        let err = c_error_to_trace_error(-5, "trace_read");
        assert!(matches!(err, TraceError::FileSystemError { .. }));
        assert!(err.to_string().contains("Permission denied"));

        let err = c_error_to_trace_error(-6, "trace_read");
        assert!(matches!(err, TraceError::EndOfTrace));

        let err = c_error_to_trace_error(-7, "trace_parse");
        assert!(matches!(err, TraceError::InvalidFormat { .. }));

        let err = c_error_to_trace_error(-2, "trace_init");
        assert!(matches!(err, TraceError::InvalidParameter { .. }));
    }

    #[test]
    fn test_check_c_result() {
        // Test successful result
        assert!(check_c_result_cache(0, "test_function").is_ok());
        assert!(check_c_result_trace(0, "test_function").is_ok());

        // Test error results
        let cache_result = check_c_result_cache(-3, "cache_create");
        assert!(cache_result.is_err());
        assert!(matches!(cache_result.unwrap_err(), CacheError::OutOfMemory));

        let trace_result = check_c_result_trace(-4, "trace_open");
        assert!(trace_result.is_err());
        assert!(matches!(trace_result.unwrap_err(), TraceError::FileSystemError { .. }));
    }

    #[test]
    fn test_null_ptr_checks() {
        let valid_ptr = &mut 42 as *mut i32;
        let null_ptr = std::ptr::null_mut::<i32>();

        // Test valid pointer
        assert!(check_null_ptr_cache(valid_ptr, "test").is_ok());
        assert!(check_null_ptr_trace(valid_ptr, "test").is_ok());

        // Test null pointer
        let cache_result = check_null_ptr_cache(null_ptr, "cache_create");
        assert!(cache_result.is_err());
        assert!(matches!(cache_result.unwrap_err(), CacheError::NullPointer { .. }));

        let trace_result = check_null_ptr_trace(null_ptr, "reader_create");
        assert!(trace_result.is_err());
        assert!(matches!(trace_result.unwrap_err(), TraceError::NullPointer { .. }));
    }

    #[test]
    fn test_validation_functions() {
        // Test cache capacity validation
        assert!(validate_cache_capacity(1024).is_ok());
        assert!(validate_cache_capacity(0).is_err());
        assert!(validate_cache_capacity(u64::MAX).is_err());

        // Test object size validation
        assert!(validate_object_size(1024).is_ok());
        assert!(validate_object_size(0).is_err());
        assert!(validate_object_size(-1).is_err());
        assert!(validate_object_size(i64::MAX).is_err());

        // Test file path validation
        assert!(validate_file_path("valid/path.txt").is_ok());
        assert!(validate_file_path("").is_err());
        assert!(validate_file_path("path\0with\0nulls").is_err());
    }

    #[test]
    fn test_c_string_conversion() {
        use std::ffi::CString;

        // Test valid C string
        let c_str = CString::new("hello world").unwrap();
        let result = unsafe { c_string_to_rust_safe(c_str.as_ptr()) };
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), Some("hello world".to_string()));

        // Test null pointer
        let result = unsafe { c_string_to_rust_safe(std::ptr::null()) };
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), None);
    }

    #[test]
    fn test_handle_c_init_result() {
        let valid_ptr = &mut 42 as *mut i32;
        let null_ptr = std::ptr::null_mut::<i32>();

        // Test valid pointer
        let result = handle_c_init_result(valid_ptr, "test_init", false);
        assert!(result.is_ok());
        assert_eq!(result.unwrap(), valid_ptr);

        // Test null pointer without errno check
        let result = handle_c_init_result(null_ptr, "test_init", false);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), CacheError::InitializationFailed { .. }));

        // Test null pointer with errno check
        let result = handle_c_init_result(null_ptr, "test_init", true);
        assert!(result.is_err());
        assert!(matches!(result.unwrap_err(), CacheError::OutOfMemory));
    }

    #[test]
    fn test_result_type_aliases() {
        // Test that our type aliases work correctly
        let cache_result: CacheResult<i32> = Ok(42);
        assert!(cache_result.is_ok());
        assert_eq!(cache_result.unwrap(), 42);

        let trace_result: TraceResult<String> = Ok("test".to_string());
        assert!(trace_result.is_ok());
        assert_eq!(trace_result.unwrap(), "test");

        let generic_result: Result<bool> = Ok(true);
        assert!(generic_result.is_ok());
        assert!(generic_result.unwrap());
    }
}
