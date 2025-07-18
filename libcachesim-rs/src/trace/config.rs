//! Trace configuration and format definitions

use crate::ffi::bindings::trace_type_e;
use crate::error::{TraceError, Result};

/// Configuration for trace processing
#[derive(Debug, Clone)]
pub struct TraceConfig {
    /// Object ID type in the trace
    pub obj_id_type: ObjectIdType,
    /// Whether to ignore object size (use default size)
    pub ignore_size: bool,
    /// Default object size when size is ignored or missing
    pub default_size: u64,
    /// Whether to consider TTL information
    pub consider_ttl: bool,
    /// Default TTL in seconds
    pub default_ttl: u64,
}

impl Default for TraceConfig {
    fn default() -> Self {
        Self {
            obj_id_type: ObjectIdType::Numeric,
            ignore_size: false,
            default_size: 1,
            consider_ttl: false,
            default_ttl: 86400, // 1 day
        }
    }
}

/// Supported trace file formats matching libCacheSim's trace_type_e
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum TraceType {
    /// Comma-separated values format
    Csv,
    /// Binary format
    Binary,
    /// Plain text format (space/tab separated)
    PlainText,
    /// Oracle general trace format
    OracleGeneral,
    /// LCS (libCacheSim) format
    Lcs,
    /// VSCSI trace format
    Vscsi,
    /// Twitter cluster trace format
    TwitterCluster,
    /// Twitter cluster with namespace trace format
    TwitterClusterNs,
    /// Oracle simulation Twitter trace format
    OracleSimTwitter,
    /// Oracle system Twitter trace format
    OracleSysTwitter,
    /// Oracle simulation Twitter with namespace trace format
    OracleSimTwitterNs,
    /// Oracle system Twitter with namespace trace format
    OracleSysTwitterNs,
    /// Valpin trace format
    Valpin,
}

impl TraceType {
    /// Convert Rust TraceType to C trace_type_e
    pub fn to_c_trace_type(&self) -> trace_type_e {
        match self {
            TraceType::Csv => trace_type_e::CSV_TRACE,
            TraceType::Binary => trace_type_e::BIN_TRACE,
            TraceType::PlainText => trace_type_e::PLAIN_TXT_TRACE,
            TraceType::OracleGeneral => trace_type_e::ORACLE_GENERAL_TRACE,
            TraceType::Lcs => trace_type_e::LCS_TRACE,
            TraceType::Vscsi => trace_type_e::VSCSI_TRACE,
            TraceType::TwitterCluster => trace_type_e::TWR_TRACE,
            TraceType::TwitterClusterNs => trace_type_e::TWRNS_TRACE,
            TraceType::OracleSimTwitter => trace_type_e::ORACLE_SIM_TWR_TRACE,
            TraceType::OracleSysTwitter => trace_type_e::ORACLE_SYS_TWR_TRACE,
            TraceType::OracleSimTwitterNs => trace_type_e::ORACLE_SIM_TWRNS_TRACE,
            TraceType::OracleSysTwitterNs => trace_type_e::ORACLE_SYS_TWRNS_TRACE,
            TraceType::Valpin => trace_type_e::VALPIN_TRACE,
        }
    }

    /// Convert C trace_type_e to Rust TraceType
    pub fn from_c_trace_type(c_type: trace_type_e) -> Result<Self, TraceError> {
        match c_type {
            trace_type_e::CSV_TRACE => Ok(TraceType::Csv),
            trace_type_e::BIN_TRACE => Ok(TraceType::Binary),
            trace_type_e::PLAIN_TXT_TRACE => Ok(TraceType::PlainText),
            trace_type_e::ORACLE_GENERAL_TRACE => Ok(TraceType::OracleGeneral),
            trace_type_e::LCS_TRACE => Ok(TraceType::Lcs),
            trace_type_e::VSCSI_TRACE => Ok(TraceType::Vscsi),
            trace_type_e::TWR_TRACE => Ok(TraceType::TwitterCluster),
            trace_type_e::TWRNS_TRACE => Ok(TraceType::TwitterClusterNs),
            trace_type_e::ORACLE_SIM_TWR_TRACE => Ok(TraceType::OracleSimTwitter),
            trace_type_e::ORACLE_SYS_TWR_TRACE => Ok(TraceType::OracleSysTwitter),
            trace_type_e::ORACLE_SIM_TWRNS_TRACE => Ok(TraceType::OracleSimTwitterNs),
            trace_type_e::ORACLE_SYS_TWRNS_TRACE => Ok(TraceType::OracleSysTwitterNs),
            trace_type_e::VALPIN_TRACE => Ok(TraceType::Valpin),
            trace_type_e::UNKNOWN_TRACE => Err(TraceError::UnsupportedTraceType {
                trace_type: "UNKNOWN_TRACE".to_string(),
            }),
        }
    }

    /// Get the string representation used by libCacheSim
    pub fn as_str(&self) -> &'static str {
        match self {
            TraceType::Csv => "csv",
            TraceType::Binary => "bin",
            TraceType::PlainText => "txt",
            TraceType::OracleGeneral => "oracle",
            TraceType::Lcs => "lcs",
            TraceType::Vscsi => "vscsi",
            TraceType::TwitterCluster => "twr",
            TraceType::TwitterClusterNs => "twrns",
            TraceType::OracleSimTwitter => "oracle_sim_twr",
            TraceType::OracleSysTwitter => "oracle_sys_twr",
            TraceType::OracleSimTwitterNs => "oracle_sim_twrns",
            TraceType::OracleSysTwitterNs => "oracle_sys_twrns",
            TraceType::Valpin => "valpin",
        }
    }

    /// Parse trace type from string
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_lowercase().as_str() {
            "csv" => Some(TraceType::Csv),
            "bin" | "binary" => Some(TraceType::Binary),
            "txt" | "text" | "plain" => Some(TraceType::PlainText),
            "oracle" | "oracle_general" => Some(TraceType::OracleGeneral),
            "lcs" => Some(TraceType::Lcs),
            "vscsi" => Some(TraceType::Vscsi),
            "twr" | "twitter" => Some(TraceType::TwitterCluster),
            "twrns" | "twitter_ns" => Some(TraceType::TwitterClusterNs),
            "oracle_sim_twr" => Some(TraceType::OracleSimTwitter),
            "oracle_sys_twr" => Some(TraceType::OracleSysTwitter),
            "oracle_sim_twrns" => Some(TraceType::OracleSimTwitterNs),
            "oracle_sys_twrns" => Some(TraceType::OracleSysTwitterNs),
            "valpin" => Some(TraceType::Valpin),
            _ => None,
        }
    }

    /// Get file extensions commonly associated with this trace type
    pub fn extensions(&self) -> &'static [&'static str] {
        match self {
            TraceType::Csv => &["csv"],
            TraceType::Binary => &["bin", "binary"],
            TraceType::PlainText => &["txt", "text"],
            TraceType::OracleGeneral => &["oracle"],
            TraceType::Lcs => &["lcs"],
            TraceType::Vscsi => &["vscsi"],
            TraceType::TwitterCluster => &["twr"],
            TraceType::TwitterClusterNs => &["twrns"],
            TraceType::OracleSimTwitter => &["oracle_sim_twr"],
            TraceType::OracleSysTwitter => &["oracle_sys_twr"],
            TraceType::OracleSimTwitterNs => &["oracle_sim_twrns"],
            TraceType::OracleSysTwitterNs => &["oracle_sys_twrns"],
            TraceType::Valpin => &["valpin"],
        }
    }

    /// Check if this trace type supports compression
    pub fn supports_compression(&self) -> bool {
        match self {
            TraceType::Csv | TraceType::PlainText | TraceType::Lcs => true,
            _ => false,
        }
    }

    /// Get description of the trace format
    pub fn description(&self) -> &'static str {
        match self {
            TraceType::Csv => "Comma-separated values with headers",
            TraceType::Binary => "Binary format for efficient storage",
            TraceType::PlainText => "Space or tab separated plain text",
            TraceType::OracleGeneral => "Oracle general trace format",
            TraceType::Lcs => "libCacheSim native format",
            TraceType::Vscsi => "VMware VSCSI trace format",
            TraceType::TwitterCluster => "Twitter cluster trace format",
            TraceType::TwitterClusterNs => "Twitter cluster with namespace trace format",
            TraceType::OracleSimTwitter => "Oracle simulation Twitter trace format",
            TraceType::OracleSysTwitter => "Oracle system Twitter trace format",
            TraceType::OracleSimTwitterNs => "Oracle simulation Twitter with namespace trace format",
            TraceType::OracleSysTwitterNs => "Oracle system Twitter with namespace trace format",
            TraceType::Valpin => "Valpin trace format",
        }
    }

    /// Get all available trace types
    pub fn all() -> &'static [TraceType] {
        &[
            TraceType::Csv,
            TraceType::Binary,
            TraceType::PlainText,
            TraceType::OracleGeneral,
            TraceType::Lcs,
            TraceType::Vscsi,
            TraceType::TwitterCluster,
            TraceType::TwitterClusterNs,
            TraceType::OracleSimTwitter,
            TraceType::OracleSysTwitter,
            TraceType::OracleSimTwitterNs,
            TraceType::OracleSysTwitterNs,
            TraceType::Valpin,
        ]
    }

    /// Check if this is a Twitter-based trace format
    pub fn is_twitter_trace(&self) -> bool {
        matches!(
            self,
            TraceType::TwitterCluster
                | TraceType::TwitterClusterNs
                | TraceType::OracleSimTwitter
                | TraceType::OracleSysTwitter
                | TraceType::OracleSimTwitterNs
                | TraceType::OracleSysTwitterNs
        )
    }

    /// Check if this is an Oracle-based trace format
    pub fn is_oracle_trace(&self) -> bool {
        matches!(
            self,
            TraceType::OracleGeneral
                | TraceType::OracleSimTwitter
                | TraceType::OracleSysTwitter
                | TraceType::OracleSimTwitterNs
                | TraceType::OracleSysTwitterNs
        )
    }
}

/// Object ID type in trace files
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum ObjectIdType {
    /// Numeric object IDs (most common)
    Numeric,
    /// String object IDs
    String,
    /// Raw bytes object IDs
    Bytes,
}

impl ObjectIdType {
    /// Get string representation
    pub fn as_str(&self) -> &'static str {
        match self {
            ObjectIdType::Numeric => "numeric",
            ObjectIdType::String => "string",
            ObjectIdType::Bytes => "bytes",
        }
    }

    /// Parse from string
    pub fn from_str(s: &str) -> Option<Self> {
        match s.to_lowercase().as_str() {
            "numeric" | "num" | "int" | "integer" => Some(ObjectIdType::Numeric),
            "string" | "str" | "text" => Some(ObjectIdType::String),
            "bytes" | "byte" | "binary" => Some(ObjectIdType::Bytes),
            _ => None,
        }
    }
}

impl std::fmt::Display for TraceType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

impl std::str::FromStr for TraceType {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        TraceType::from_str(s).ok_or_else(|| format!("Unknown trace type: {}", s))
    }
}

impl std::fmt::Display for ObjectIdType {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.as_str())
    }
}

impl std::str::FromStr for ObjectIdType {
    type Err = String;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        ObjectIdType::from_str(s).ok_or_else(|| format!("Unknown object ID type: {}", s))
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn test_trace_config_default() {
        let config = TraceConfig::default();
        assert_eq!(config.obj_id_type, ObjectIdType::Numeric);
        assert!(!config.ignore_size);
        assert_eq!(config.default_size, 1);
        assert!(!config.consider_ttl);
        assert_eq!(config.default_ttl, 86400);
    }

    #[test]
    fn test_trace_type_conversions() {
        assert_eq!(TraceType::Csv.as_str(), "csv");
        assert_eq!(TraceType::from_str("csv"), Some(TraceType::Csv));
        assert_eq!(TraceType::from_str("CSV"), Some(TraceType::Csv));
        assert_eq!(TraceType::from_str("invalid"), None);
    }

    #[test]
    fn test_trace_type_properties() {
        assert!(TraceType::Csv.supports_compression());
        assert!(!TraceType::Binary.supports_compression());

        let extensions = TraceType::Csv.extensions();
        assert!(extensions.contains(&"csv"));
    }

    #[test]
    fn test_object_id_type() {
        assert_eq!(ObjectIdType::Numeric.as_str(), "numeric");
        assert_eq!(ObjectIdType::from_str("string"), Some(ObjectIdType::String));
        assert_eq!(ObjectIdType::from_str("invalid"), None);
    }

    #[test]
    fn test_all_trace_types() {
        let types = TraceType::all();
        assert!(types.len() > 5);
        assert!(types.contains(&TraceType::Csv));
        assert!(types.contains(&TraceType::Binary));
    }

    #[test]
    fn test_trace_type_c_conversions() {
        // Test round-trip conversion
        let trace_type = TraceType::Csv;
        let c_type = trace_type.to_c_trace_type();
        let back_to_rust = TraceType::from_c_trace_type(c_type).unwrap();
        assert_eq!(trace_type, back_to_rust);

        // Test all supported types
        for &trace_type in TraceType::all() {
            let c_type = trace_type.to_c_trace_type();
            let back_to_rust = TraceType::from_c_trace_type(c_type).unwrap();
            assert_eq!(trace_type, back_to_rust);
        }
    }

    #[test]
    fn test_trace_type_categories() {
        assert!(TraceType::TwitterCluster.is_twitter_trace());
        assert!(TraceType::TwitterClusterNs.is_twitter_trace());
        assert!(!TraceType::Csv.is_twitter_trace());

        assert!(TraceType::OracleGeneral.is_oracle_trace());
        assert!(TraceType::OracleSimTwitter.is_oracle_trace());
        assert!(!TraceType::Csv.is_oracle_trace());
    }

    #[test]
    fn test_trace_type_descriptions() {
        for &trace_type in TraceType::all() {
            let description = trace_type.description();
            assert!(!description.is_empty());
            assert!(description.len() > 10); // Should be descriptive
        }
    }
}
