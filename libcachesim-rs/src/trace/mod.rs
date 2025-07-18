//! Trace processing functionality for cache simulation

pub mod reader;
pub mod request;
pub mod config;

pub use reader::TraceReader;
pub use request::{CacheRequest, Operation};
pub use config::{TraceConfig, TraceType};

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_trace_types_exist() {
        // Basic smoke test for type definitions
        let _config = TraceConfig::default();
        let _trace_type = TraceType::Csv;
    }
}
