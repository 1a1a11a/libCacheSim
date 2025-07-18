//! Cache eviction algorithm definitions

/// Supported cache eviction algorithms
///
/// This enum represents all the eviction algorithms supported by libCacheSim.
/// Each algorithm has different performance characteristics and use cases.
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum EvictionAlgorithm {
    /// Least Recently Used - evicts the least recently accessed item
    Lru,
    /// LRU version 0 (legacy implementation)
    LruV0,
    /// Least Frequently Used - evicts the least frequently accessed item
    Lfu,
    /// LFU C++ implementation
    LfuCpp,
    /// LFUDA - LFU with Dynamic Aging
    Lfuda,
    /// First In, First Out - evicts the oldest item
    Fifo,
    /// FIFO with merge operations
    FifoMerge,
    /// FIFO with reinsertion capability
    FifoReinsertion,
    /// Adaptive Replacement Cache - balances recency and frequency
    Arc,
    /// ARC version 0 (legacy implementation)
    ArcV0,
    /// Clock algorithm - approximates LRU with lower overhead
    Clock,
    /// Clock-Pro - enhanced clock algorithm
    ClockPro,
    /// Simple, Scalable, and Effective FIFO-based eviction
    S3Fifo,
    /// S3-FIFO with dynamic sizing
    S3FifoDynamic,
    /// S3-FIFO version 0 (legacy implementation)
    S3FifoV0,
    /// Simple FIFO implementation
    SFifo,
    /// Simple FIFO version 0
    SFifoV0,
    /// S3-LRU algorithm
    S3Lru,
    /// SIEVE - high-performance eviction algorithm
    Sieve,
    /// Segmented LRU
    Slru,
    /// Segmented LRU version 0
    SlruV0,
    /// Most Recently Used - evicts the most recently accessed item
    Mru,
    /// Random eviction
    Random,
    /// Two Random - evicts the less recently used of two random items
    RandomTwo,
    /// Random LRU hybrid
    RandomLru,
    /// LRU with probabilistic operations
    LruProb,
    /// Two Queue algorithm
    TwoQ,
    /// Low Inter-reference Recency Set
    Lirs,
    /// Hyperbolic caching
    Hyperbolic,
    /// Size-based eviction
    Size,
    /// CAR (Clock with Adaptive Replacement)
    Car,
    /// Cacheus algorithm
    Cacheus,
    /// CR-LFU (Clock-based LFU)
    CrLfu,
    /// GDSF (Greedy Dual Size Frequency)
    Gdsf,
    /// LeCaR (Learning Cache Replacement)
    LeCar,
    /// LeCaR version 0
    LeCarV0,
    /// LHD (Least Hit Density)
    Lhd,
    /// QDLP (Quick Demotion Lazy Promotion)
    Qdlp,
    /// SR-LRU (Set-associative LRU)
    SrLru,
    /// W-TinyLFU (Window Tiny LFU)
    WTinyLfu,
    /// Belady's optimal algorithm (requires future knowledge)
    Belady,
    /// Belady with size consideration
    BeladySize,
    /// Flash-aware probabilistic algorithm
    FlashProb,
    /// No eviction (cache never evicts, fails when full)
    Nop,
    /// Plugin-based custom cache
    Plugin,
    /// Three-level cache (requires ENABLE_3L_CACHE)
    #[cfg(feature = "3l-cache")]
    ThreeL,
    /// LRB (Learning-based cache replacement, requires ENABLE_LRB)
    #[cfg(feature = "lrb")]
    Lrb,
    /// GLCache (machine learning cache, requires ENABLE_GLCACHE)
    #[cfg(feature = "glcache")]
    GlCache,
}

impl EvictionAlgorithm {
    /// Get the string name of the algorithm as used by libCacheSim
    pub fn name(&self) -> &'static str {
        match self {
            EvictionAlgorithm::Lru => "LRU",
            EvictionAlgorithm::LruV0 => "LRUv0",
            EvictionAlgorithm::Lfu => "LFU",
            EvictionAlgorithm::LfuCpp => "LFUCpp",
            EvictionAlgorithm::Lfuda => "LFUDA",
            EvictionAlgorithm::Fifo => "FIFO",
            EvictionAlgorithm::FifoMerge => "FIFO_Merge",
            EvictionAlgorithm::FifoReinsertion => "FIFO_Reinsertion",
            EvictionAlgorithm::Arc => "ARC",
            EvictionAlgorithm::ArcV0 => "ARCv0",
            EvictionAlgorithm::Clock => "Clock",
            EvictionAlgorithm::ClockPro => "ClockPro",
            EvictionAlgorithm::S3Fifo => "S3FIFO",
            EvictionAlgorithm::S3FifoDynamic => "S3FIFOd",
            EvictionAlgorithm::S3FifoV0 => "S3FIFOv0",
            EvictionAlgorithm::SFifo => "SFIFO",
            EvictionAlgorithm::SFifoV0 => "SFIFOv0",
            EvictionAlgorithm::S3Lru => "S3LRU",
            EvictionAlgorithm::Sieve => "Sieve",
            EvictionAlgorithm::Slru => "SLRU",
            EvictionAlgorithm::SlruV0 => "SLRUv0",
            EvictionAlgorithm::Mru => "MRU",
            EvictionAlgorithm::Random => "Random",
            EvictionAlgorithm::RandomTwo => "RandomTwo",
            EvictionAlgorithm::RandomLru => "RandomLRU",
            EvictionAlgorithm::LruProb => "LRU_Prob",
            EvictionAlgorithm::TwoQ => "TwoQ",
            EvictionAlgorithm::Lirs => "LIRS",
            EvictionAlgorithm::Hyperbolic => "Hyperbolic",
            EvictionAlgorithm::Size => "Size",
            EvictionAlgorithm::Car => "CAR",
            EvictionAlgorithm::Cacheus => "Cacheus",
            EvictionAlgorithm::CrLfu => "CR_LFU",
            EvictionAlgorithm::Gdsf => "GDSF",
            EvictionAlgorithm::LeCar => "LeCaR",
            EvictionAlgorithm::LeCarV0 => "LeCaRv0",
            EvictionAlgorithm::Lhd => "LHD",
            EvictionAlgorithm::Qdlp => "QDLP",
            EvictionAlgorithm::SrLru => "SR_LRU",
            EvictionAlgorithm::WTinyLfu => "WTinyLFU",
            EvictionAlgorithm::Belady => "Belady",
            EvictionAlgorithm::BeladySize => "BeladySize",
            EvictionAlgorithm::FlashProb => "flashProb",
            EvictionAlgorithm::Nop => "nop",
            EvictionAlgorithm::Plugin => "pluginCache",
            #[cfg(feature = "3l-cache")]
            EvictionAlgorithm::ThreeL => "ThreeLCache",
            #[cfg(feature = "lrb")]
            EvictionAlgorithm::Lrb => "LRB",
            #[cfg(feature = "glcache")]
            EvictionAlgorithm::GlCache => "GLCache",
        }
    }

    /// Check if this algorithm supports TTL (Time To Live)
    pub fn supports_ttl(&self) -> bool {
        // Most algorithms support TTL, but some specialized ones might not
        match self {
            EvictionAlgorithm::Nop => false,
            _ => true,
        }
    }

    /// Check if this algorithm requires object size information
    pub fn requires_size(&self) -> bool {
        match self {
            EvictionAlgorithm::Size => true,
            EvictionAlgorithm::Hyperbolic => true,
            _ => false,
        }
    }

    /// Get a description of the algorithm
    pub fn description(&self) -> &'static str {
        match self {
            EvictionAlgorithm::Lru => "Least Recently Used - evicts least recently accessed items",
            EvictionAlgorithm::LruV0 => "LRU version 0 - legacy LRU implementation",
            EvictionAlgorithm::Lfu => "Least Frequently Used - evicts least frequently accessed items",
            EvictionAlgorithm::LfuCpp => "LFU C++ implementation - optimized LFU algorithm",
            EvictionAlgorithm::Lfuda => "LFU with Dynamic Aging - LFU with aging to handle changing workloads",
            EvictionAlgorithm::Fifo => "First In, First Out - evicts oldest items",
            EvictionAlgorithm::FifoMerge => "FIFO with merge operations - FIFO with object merging capability",
            EvictionAlgorithm::FifoReinsertion => "FIFO with reinsertion - FIFO allowing object reinsertion",
            EvictionAlgorithm::Arc => "Adaptive Replacement Cache - balances recency and frequency",
            EvictionAlgorithm::ArcV0 => "ARC version 0 - legacy ARC implementation",
            EvictionAlgorithm::Clock => "Clock algorithm - approximates LRU with lower overhead",
            EvictionAlgorithm::ClockPro => "Clock-Pro - enhanced clock algorithm with better hit rates",
            EvictionAlgorithm::S3Fifo => "S3-FIFO - Simple, Scalable, and Effective FIFO-based eviction",
            EvictionAlgorithm::S3FifoDynamic => "S3-FIFO with dynamic sizing adaptation",
            EvictionAlgorithm::S3FifoV0 => "S3-FIFO version 0 - legacy S3-FIFO implementation",
            EvictionAlgorithm::SFifo => "Simple FIFO - basic FIFO implementation",
            EvictionAlgorithm::SFifoV0 => "Simple FIFO version 0 - legacy simple FIFO",
            EvictionAlgorithm::S3Lru => "S3-LRU - Simple, Scalable, and Effective LRU variant",
            EvictionAlgorithm::Sieve => "SIEVE - high-performance eviction with lazy promotion",
            EvictionAlgorithm::Slru => "Segmented LRU - LRU with multiple segments",
            EvictionAlgorithm::SlruV0 => "Segmented LRU version 0 - legacy SLRU implementation",
            EvictionAlgorithm::Mru => "Most Recently Used - evicts most recently accessed items",
            EvictionAlgorithm::Random => "Random eviction - evicts random items",
            EvictionAlgorithm::RandomTwo => "Random Two - evicts LRU of two random items",
            EvictionAlgorithm::RandomLru => "Random LRU hybrid algorithm",
            EvictionAlgorithm::LruProb => "LRU with probabilistic operations - probabilistic LRU variant",
            EvictionAlgorithm::TwoQ => "Two Queue algorithm with hot and cold queues",
            EvictionAlgorithm::Lirs => "Low Inter-reference Recency Set algorithm",
            EvictionAlgorithm::Hyperbolic => "Hyperbolic caching algorithm",
            EvictionAlgorithm::Size => "Size-based eviction - evicts largest items",
            EvictionAlgorithm::Car => "CAR - Clock with Adaptive Replacement",
            EvictionAlgorithm::Cacheus => "Cacheus algorithm - advanced caching strategy",
            EvictionAlgorithm::CrLfu => "CR-LFU - Clock-based LFU algorithm",
            EvictionAlgorithm::Gdsf => "GDSF - Greedy Dual Size Frequency algorithm",
            EvictionAlgorithm::LeCar => "LeCaR - Learning Cache Replacement algorithm",
            EvictionAlgorithm::LeCarV0 => "LeCaR version 0 - legacy LeCaR implementation",
            EvictionAlgorithm::Lhd => "LHD - Least Hit Density algorithm",
            EvictionAlgorithm::Qdlp => "QDLP - Quick Demotion Lazy Promotion algorithm",
            EvictionAlgorithm::SrLru => "SR-LRU - Set-associative LRU algorithm",
            EvictionAlgorithm::WTinyLfu => "W-TinyLFU - Window Tiny LFU algorithm",
            EvictionAlgorithm::Belady => "Belady's optimal algorithm - requires future knowledge",
            EvictionAlgorithm::BeladySize => "Belady with size consideration - optimal with size awareness",
            EvictionAlgorithm::FlashProb => "Flash-aware probabilistic algorithm - optimized for flash storage",
            EvictionAlgorithm::Nop => "No eviction - cache fails when full",
            EvictionAlgorithm::Plugin => "Plugin-based custom cache - user-defined eviction logic",
            #[cfg(feature = "3l-cache")]
            EvictionAlgorithm::ThreeL => "Three-level cache - hierarchical caching system",
            #[cfg(feature = "lrb")]
            EvictionAlgorithm::Lrb => "LRB - Learning-based cache replacement with ML",
            #[cfg(feature = "glcache")]
            EvictionAlgorithm::GlCache => "GLCache - machine learning cache algorithm",
        }
    }

    /// Get all available algorithms
    pub fn all() -> &'static [EvictionAlgorithm] {
        &[
            EvictionAlgorithm::Lru,
            EvictionAlgorithm::LruV0,
            EvictionAlgorithm::Lfu,
            EvictionAlgorithm::LfuCpp,
            EvictionAlgorithm::Lfuda,
            EvictionAlgorithm::Fifo,
            EvictionAlgorithm::FifoMerge,
            EvictionAlgorithm::FifoReinsertion,
            EvictionAlgorithm::Arc,
            EvictionAlgorithm::ArcV0,
            EvictionAlgorithm::Clock,
            EvictionAlgorithm::ClockPro,
            EvictionAlgorithm::S3Fifo,
            EvictionAlgorithm::S3FifoDynamic,
            EvictionAlgorithm::S3FifoV0,
            EvictionAlgorithm::SFifo,
            EvictionAlgorithm::SFifoV0,
            EvictionAlgorithm::S3Lru,
            EvictionAlgorithm::Sieve,
            EvictionAlgorithm::Slru,
            EvictionAlgorithm::SlruV0,
            EvictionAlgorithm::Mru,
            EvictionAlgorithm::Random,
            EvictionAlgorithm::RandomTwo,
            EvictionAlgorithm::RandomLru,
            EvictionAlgorithm::LruProb,
            EvictionAlgorithm::TwoQ,
            EvictionAlgorithm::Lirs,
            EvictionAlgorithm::Hyperbolic,
            EvictionAlgorithm::Size,
            EvictionAlgorithm::Car,
            EvictionAlgorithm::Cacheus,
            EvictionAlgorithm::CrLfu,
            EvictionAlgorithm::Gdsf,
            EvictionAlgorithm::LeCar,
            EvictionAlgorithm::LeCarV0,
            EvictionAlgorithm::Lhd,
            EvictionAlgorithm::Qdlp,
            EvictionAlgorithm::SrLru,
            EvictionAlgorithm::WTinyLfu,
            EvictionAlgorithm::Belady,
            EvictionAlgorithm::BeladySize,
            EvictionAlgorithm::FlashProb,
            EvictionAlgorithm::Nop,
            EvictionAlgorithm::Plugin,
        ]
    }

    /// Parse an algorithm from its string name
    ///
    /// This function parses algorithm names as used by libCacheSim.
    /// It's case-insensitive and supports common variations.
    pub fn from_name(name: &str) -> Option<Self> {
        let name_lower = name.to_lowercase();
        match name_lower.as_str() {
            "lru" => Some(EvictionAlgorithm::Lru),
            "lruv0" | "lru_v0" => Some(EvictionAlgorithm::LruV0),
            "lfu" => Some(EvictionAlgorithm::Lfu),
            "lfucpp" | "lfu_cpp" => Some(EvictionAlgorithm::LfuCpp),
            "lfuda" => Some(EvictionAlgorithm::Lfuda),
            "fifo" => Some(EvictionAlgorithm::Fifo),
            "fifo_merge" | "fifomerge" => Some(EvictionAlgorithm::FifoMerge),
            "fifo_reinsertion" | "fiforeinsertion" => Some(EvictionAlgorithm::FifoReinsertion),
            "arc" => Some(EvictionAlgorithm::Arc),
            "arcv0" | "arc_v0" => Some(EvictionAlgorithm::ArcV0),
            "clock" => Some(EvictionAlgorithm::Clock),
            "clockpro" | "clock_pro" => Some(EvictionAlgorithm::ClockPro),
            "s3fifo" | "s3_fifo" => Some(EvictionAlgorithm::S3Fifo),
            "s3fifod" | "s3_fifo_d" | "s3fifodynamic" => Some(EvictionAlgorithm::S3FifoDynamic),
            "s3fifov0" | "s3_fifo_v0" => Some(EvictionAlgorithm::S3FifoV0),
            "sfifo" | "s_fifo" => Some(EvictionAlgorithm::SFifo),
            "sfifov0" | "s_fifo_v0" => Some(EvictionAlgorithm::SFifoV0),
            "s3lru" | "s3_lru" => Some(EvictionAlgorithm::S3Lru),
            "sieve" => Some(EvictionAlgorithm::Sieve),
            "slru" | "s_lru" => Some(EvictionAlgorithm::Slru),
            "slruv0" | "s_lru_v0" => Some(EvictionAlgorithm::SlruV0),
            "mru" => Some(EvictionAlgorithm::Mru),
            "random" => Some(EvictionAlgorithm::Random),
            "randomtwo" | "random_two" => Some(EvictionAlgorithm::RandomTwo),
            "randomlru" | "random_lru" => Some(EvictionAlgorithm::RandomLru),
            "lru_prob" | "lruprob" => Some(EvictionAlgorithm::LruProb),
            "twoq" | "two_q" | "2q" => Some(EvictionAlgorithm::TwoQ),
            "lirs" => Some(EvictionAlgorithm::Lirs),
            "hyperbolic" => Some(EvictionAlgorithm::Hyperbolic),
            "size" => Some(EvictionAlgorithm::Size),
            "car" => Some(EvictionAlgorithm::Car),
            "cacheus" => Some(EvictionAlgorithm::Cacheus),
            "cr_lfu" | "crlfu" => Some(EvictionAlgorithm::CrLfu),
            "gdsf" => Some(EvictionAlgorithm::Gdsf),
            "lecar" => Some(EvictionAlgorithm::LeCar),
            "lecarv0" | "lecar_v0" => Some(EvictionAlgorithm::LeCarV0),
            "lhd" => Some(EvictionAlgorithm::Lhd),
            "qdlp" => Some(EvictionAlgorithm::Qdlp),
            "sr_lru" | "srlru" => Some(EvictionAlgorithm::SrLru),
            "wtinylfu" | "w_tiny_lfu" => Some(EvictionAlgorithm::WTinyLfu),
            "belady" => Some(EvictionAlgorithm::Belady),
            "beladysize" | "belady_size" => Some(EvictionAlgorithm::BeladySize),
            "flashprob" | "flash_prob" => Some(EvictionAlgorithm::FlashProb),
            "nop" | "no_op" => Some(EvictionAlgorithm::Nop),
            "plugin" | "plugincache" => Some(EvictionAlgorithm::Plugin),
            #[cfg(feature = "3l-cache")]
            "threelcache" | "three_l_cache" | "3lcache" => Some(EvictionAlgorithm::ThreeL),
            #[cfg(feature = "lrb")]
            "lrb" => Some(EvictionAlgorithm::Lrb),
            #[cfg(feature = "glcache")]
            "glcache" | "gl_cache" => Some(EvictionAlgorithm::GlCache),
            _ => None,
        }
    }

    /// Check if this algorithm is available in the current build
    ///
    /// Some algorithms require specific features to be enabled.
    pub fn is_available(&self) -> bool {
        match self {
            #[cfg(feature = "3l-cache")]
            EvictionAlgorithm::ThreeL => true,
            #[cfg(feature = "lrb")]
            EvictionAlgorithm::Lrb => true,
            #[cfg(feature = "glcache")]
            EvictionAlgorithm::GlCache => true,
            _ => true,
        }
    }

    /// Get the category of this algorithm
    pub fn category(&self) -> AlgorithmCategory {
        match self {
            EvictionAlgorithm::Lru | EvictionAlgorithm::LruV0 | EvictionAlgorithm::LruProb => {
                AlgorithmCategory::Lru
            }
            EvictionAlgorithm::Lfu | EvictionAlgorithm::LfuCpp | EvictionAlgorithm::Lfuda => {
                AlgorithmCategory::Lfu
            }
            EvictionAlgorithm::Fifo
            | EvictionAlgorithm::FifoMerge
            | EvictionAlgorithm::FifoReinsertion
            | EvictionAlgorithm::S3Fifo
            | EvictionAlgorithm::S3FifoDynamic
            | EvictionAlgorithm::S3FifoV0
            | EvictionAlgorithm::SFifo
            | EvictionAlgorithm::SFifoV0 => AlgorithmCategory::Fifo,
            EvictionAlgorithm::Arc | EvictionAlgorithm::ArcV0 => AlgorithmCategory::Adaptive,
            EvictionAlgorithm::Clock | EvictionAlgorithm::ClockPro => AlgorithmCategory::Clock,
            EvictionAlgorithm::Sieve => AlgorithmCategory::Modern,
            EvictionAlgorithm::S3Lru => AlgorithmCategory::Modern,
            EvictionAlgorithm::Slru | EvictionAlgorithm::SlruV0 => AlgorithmCategory::Segmented,
            EvictionAlgorithm::Random | EvictionAlgorithm::RandomTwo | EvictionAlgorithm::RandomLru => {
                AlgorithmCategory::Random
            }
            EvictionAlgorithm::Belady | EvictionAlgorithm::BeladySize => AlgorithmCategory::Optimal,
            EvictionAlgorithm::Size => AlgorithmCategory::SizeBased,
            EvictionAlgorithm::Nop => AlgorithmCategory::Special,
            EvictionAlgorithm::Plugin => AlgorithmCategory::Special,
            #[cfg(feature = "3l-cache")]
            EvictionAlgorithm::ThreeL => AlgorithmCategory::MachineLearning,
            #[cfg(feature = "lrb")]
            EvictionAlgorithm::Lrb => AlgorithmCategory::MachineLearning,
            #[cfg(feature = "glcache")]
            EvictionAlgorithm::GlCache => AlgorithmCategory::MachineLearning,
            _ => AlgorithmCategory::Other,
        }
    }
}

/// Categories of eviction algorithms
#[derive(Debug, Clone, Copy, PartialEq, Eq, Hash)]
pub enum AlgorithmCategory {
    /// LRU-based algorithms
    Lru,
    /// LFU-based algorithms
    Lfu,
    /// FIFO-based algorithms
    Fifo,
    /// Adaptive algorithms (ARC, CAR, etc.)
    Adaptive,
    /// Clock-based algorithms
    Clock,
    /// Modern high-performance algorithms (SIEVE, S3-FIFO, etc.)
    Modern,
    /// Segmented algorithms (SLRU, etc.)
    Segmented,
    /// Random-based algorithms
    Random,
    /// Optimal algorithms (Belady)
    Optimal,
    /// Size-based algorithms
    SizeBased,
    /// Machine learning algorithms
    MachineLearning,
    /// Special algorithms (nop, plugin)
    Special,
    /// Other algorithms
    Other,
}

impl std::fmt::Display for EvictionAlgorithm {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{}", self.name())
    }
}

impl std::str::FromStr for EvictionAlgorithm {
    type Err = crate::error::CacheError;

    fn from_str(s: &str) -> Result<Self, Self::Err> {
        Self::from_name(s).ok_or_else(|| {
            crate::error::CacheError::unsupported_algorithm(format!(
                "Unknown eviction algorithm: {}",
                s
            ))
        })
    }
}

impl EvictionAlgorithm {
    /// Convert to a C string for use with libCacheSim functions
    ///
    /// This function returns a CString that can be passed to C functions
    /// that expect algorithm names.
    pub fn to_c_string(&self) -> Result<std::ffi::CString, crate::error::CacheError> {
        std::ffi::CString::new(self.name())
            .map_err(|_| crate::error::CacheError::invalid_operation(
                format!("Algorithm name '{}' contains null bytes", self.name())
            ))
    }

    /// Get the cache initialization function pointer for this algorithm
    ///
    /// This function returns the appropriate C function pointer for initializing
    /// a cache with this eviction algorithm.
    pub fn get_init_function(&self) -> Option<crate::ffi::sys::cache_init_func_ptr> {
        // Note: This will be implemented when we have access to the actual function pointers
        // For now, we return None to indicate that the function lookup needs to be done
        // by name using the C library's algorithm registry
        None
    }

    /// Check if this algorithm requires specific parameters
    pub fn requires_params(&self) -> bool {
        match self {
            // Some algorithms might require specific parameters
            EvictionAlgorithm::Clock => true,
            EvictionAlgorithm::ClockPro => true,
            EvictionAlgorithm::S3Fifo => true,
            EvictionAlgorithm::S3FifoDynamic => true,
            EvictionAlgorithm::Slru => true,
            EvictionAlgorithm::SlruV0 => true,
            EvictionAlgorithm::TwoQ => true,
            EvictionAlgorithm::Lirs => true,
            EvictionAlgorithm::Car => true,
            EvictionAlgorithm::LeCar => true,
            EvictionAlgorithm::LeCarV0 => true,
            EvictionAlgorithm::Lhd => true,
            EvictionAlgorithm::WTinyLfu => true,
            EvictionAlgorithm::Plugin => true,
            #[cfg(feature = "3l-cache")]
            EvictionAlgorithm::ThreeL => true,
            #[cfg(feature = "lrb")]
            EvictionAlgorithm::Lrb => true,
            #[cfg(feature = "glcache")]
            EvictionAlgorithm::GlCache => true,
            _ => false,
        }
    }

    /// Get default parameters for algorithms that support them
    pub fn default_params(&self) -> Option<String> {
        match self {
            // Return default parameter strings for algorithms that need them
            // These would be algorithm-specific configuration strings
            EvictionAlgorithm::Clock => Some("n-bit-counter=1".to_string()),
            EvictionAlgorithm::S3Fifo => Some("".to_string()), // S3-FIFO uses default params
            EvictionAlgorithm::Slru => Some("n-seg=2".to_string()),
            EvictionAlgorithm::TwoQ => Some("".to_string()),
            _ => None,
        }
    }

    /// Validate that this algorithm is compatible with the given configuration
    pub fn validate_with_config(&self, config: &crate::cache::CacheConfig) -> Result<(), crate::error::CacheError> {
        // Check TTL support
        if config.default_ttl.is_some() && !self.supports_ttl() {
            return Err(crate::error::CacheError::invalid_configuration(
                format!("Algorithm '{}' does not support TTL", self.name())
            ));
        }

        // Check size requirements
        if self.requires_size() {
            // Size-based algorithms need special handling
            // This is mainly informational for now
        }

        // Check availability
        if !self.is_available() {
            return Err(crate::error::CacheError::unsupported_algorithm(
                format!("Algorithm '{}' is not available in this build", self.name())
            ));
        }

        Ok(())
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::str::FromStr;

    #[test]
    fn test_algorithm_names() {
        assert_eq!(EvictionAlgorithm::Lru.name(), "LRU");
        assert_eq!(EvictionAlgorithm::S3Fifo.name(), "S3FIFO");
        assert_eq!(EvictionAlgorithm::Sieve.name(), "Sieve");
        assert_eq!(EvictionAlgorithm::FifoMerge.name(), "FIFO_Merge");
        assert_eq!(EvictionAlgorithm::LruProb.name(), "LRU_Prob");
        assert_eq!(EvictionAlgorithm::FlashProb.name(), "flashProb");
    }

    #[test]
    fn test_algorithm_properties() {
        assert!(EvictionAlgorithm::Lru.supports_ttl());
        assert!(!EvictionAlgorithm::Nop.supports_ttl());

        assert!(EvictionAlgorithm::Size.requires_size());
        assert!(EvictionAlgorithm::Hyperbolic.requires_size());
        assert!(!EvictionAlgorithm::Lru.requires_size());
        assert!(!EvictionAlgorithm::Fifo.requires_size());
    }

    #[test]
    fn test_all_algorithms() {
        let algorithms = EvictionAlgorithm::all();
        assert!(algorithms.len() > 10);
        assert!(algorithms.contains(&EvictionAlgorithm::Lru));
        assert!(algorithms.contains(&EvictionAlgorithm::S3Fifo));
        assert!(algorithms.contains(&EvictionAlgorithm::Sieve));
        assert!(algorithms.contains(&EvictionAlgorithm::Fifo));
        assert!(algorithms.contains(&EvictionAlgorithm::Random));
    }

    #[test]
    fn test_from_name_parsing() {
        // Test exact matches
        assert_eq!(EvictionAlgorithm::from_name("LRU"), Some(EvictionAlgorithm::Lru));
        assert_eq!(EvictionAlgorithm::from_name("lru"), Some(EvictionAlgorithm::Lru));
        assert_eq!(EvictionAlgorithm::from_name("S3FIFO"), Some(EvictionAlgorithm::S3Fifo));
        assert_eq!(EvictionAlgorithm::from_name("s3fifo"), Some(EvictionAlgorithm::S3Fifo));

        // Test variations
        assert_eq!(EvictionAlgorithm::from_name("s3_fifo"), Some(EvictionAlgorithm::S3Fifo));
        assert_eq!(EvictionAlgorithm::from_name("lru_v0"), Some(EvictionAlgorithm::LruV0));
        assert_eq!(EvictionAlgorithm::from_name("lruv0"), Some(EvictionAlgorithm::LruV0));
        assert_eq!(EvictionAlgorithm::from_name("fifo_merge"), Some(EvictionAlgorithm::FifoMerge));
        assert_eq!(EvictionAlgorithm::from_name("fifomerge"), Some(EvictionAlgorithm::FifoMerge));
        assert_eq!(EvictionAlgorithm::from_name("2q"), Some(EvictionAlgorithm::TwoQ));
        assert_eq!(EvictionAlgorithm::from_name("two_q"), Some(EvictionAlgorithm::TwoQ));

        // Test unknown algorithm
        assert_eq!(EvictionAlgorithm::from_name("unknown"), None);
        assert_eq!(EvictionAlgorithm::from_name(""), None);
    }

    #[test]
    fn test_from_str_trait() {
        // Test successful parsing
        assert_eq!(EvictionAlgorithm::from_str("LRU").unwrap(), EvictionAlgorithm::Lru);
        assert_eq!(EvictionAlgorithm::from_str("sieve").unwrap(), EvictionAlgorithm::Sieve);
        assert_eq!(EvictionAlgorithm::from_str("s3_fifo").unwrap(), EvictionAlgorithm::S3Fifo);

        // Test error case
        let result = EvictionAlgorithm::from_str("unknown_algorithm");
        assert!(result.is_err());
        assert!(result.unwrap_err().to_string().contains("Unknown eviction algorithm"));
    }

    #[test]
    fn test_display_trait() {
        assert_eq!(format!("{}", EvictionAlgorithm::Lru), "LRU");
        assert_eq!(format!("{}", EvictionAlgorithm::S3Fifo), "S3FIFO");
        assert_eq!(format!("{}", EvictionAlgorithm::Sieve), "Sieve");
    }

    #[test]
    fn test_algorithm_availability() {
        // Most algorithms should be available by default
        assert!(EvictionAlgorithm::Lru.is_available());
        assert!(EvictionAlgorithm::Fifo.is_available());
        assert!(EvictionAlgorithm::S3Fifo.is_available());
        assert!(EvictionAlgorithm::Sieve.is_available());
        assert!(EvictionAlgorithm::Random.is_available());

        // Feature-gated algorithms depend on build configuration
        // We can only test these if the features are enabled
        #[cfg(feature = "3l-cache")]
        assert!(EvictionAlgorithm::ThreeL.is_available());
        #[cfg(feature = "lrb")]
        assert!(EvictionAlgorithm::Lrb.is_available());
        #[cfg(feature = "glcache")]
        assert!(EvictionAlgorithm::GlCache.is_available());
    }

    #[test]
    fn test_algorithm_categories() {
        assert_eq!(EvictionAlgorithm::Lru.category(), AlgorithmCategory::Lru);
        assert_eq!(EvictionAlgorithm::LruV0.category(), AlgorithmCategory::Lru);
        assert_eq!(EvictionAlgorithm::LruProb.category(), AlgorithmCategory::Lru);

        assert_eq!(EvictionAlgorithm::Lfu.category(), AlgorithmCategory::Lfu);
        assert_eq!(EvictionAlgorithm::LfuCpp.category(), AlgorithmCategory::Lfu);
        assert_eq!(EvictionAlgorithm::Lfuda.category(), AlgorithmCategory::Lfu);

        assert_eq!(EvictionAlgorithm::Fifo.category(), AlgorithmCategory::Fifo);
        assert_eq!(EvictionAlgorithm::S3Fifo.category(), AlgorithmCategory::Fifo);
        assert_eq!(EvictionAlgorithm::SFifo.category(), AlgorithmCategory::Fifo);

        assert_eq!(EvictionAlgorithm::Arc.category(), AlgorithmCategory::Adaptive);
        assert_eq!(EvictionAlgorithm::ArcV0.category(), AlgorithmCategory::Adaptive);

        assert_eq!(EvictionAlgorithm::Clock.category(), AlgorithmCategory::Clock);
        assert_eq!(EvictionAlgorithm::ClockPro.category(), AlgorithmCategory::Clock);

        assert_eq!(EvictionAlgorithm::Sieve.category(), AlgorithmCategory::Modern);
        assert_eq!(EvictionAlgorithm::S3Lru.category(), AlgorithmCategory::Modern);

        assert_eq!(EvictionAlgorithm::Slru.category(), AlgorithmCategory::Segmented);
        assert_eq!(EvictionAlgorithm::SlruV0.category(), AlgorithmCategory::Segmented);

        assert_eq!(EvictionAlgorithm::Random.category(), AlgorithmCategory::Random);
        assert_eq!(EvictionAlgorithm::RandomTwo.category(), AlgorithmCategory::Random);
        assert_eq!(EvictionAlgorithm::RandomLru.category(), AlgorithmCategory::Random);

        assert_eq!(EvictionAlgorithm::Belady.category(), AlgorithmCategory::Optimal);
        assert_eq!(EvictionAlgorithm::BeladySize.category(), AlgorithmCategory::Optimal);

        assert_eq!(EvictionAlgorithm::Size.category(), AlgorithmCategory::SizeBased);

        assert_eq!(EvictionAlgorithm::Nop.category(), AlgorithmCategory::Special);
        assert_eq!(EvictionAlgorithm::Plugin.category(), AlgorithmCategory::Special);
    }

    #[test]
    fn test_algorithm_descriptions() {
        assert!(EvictionAlgorithm::Lru.description().contains("Least Recently Used"));
        assert!(EvictionAlgorithm::Lfu.description().contains("Least Frequently Used"));
        assert!(EvictionAlgorithm::Fifo.description().contains("First In, First Out"));
        assert!(EvictionAlgorithm::S3Fifo.description().contains("Simple, Scalable, and Effective"));
        assert!(EvictionAlgorithm::Sieve.description().contains("high-performance"));
        assert!(EvictionAlgorithm::Belady.description().contains("optimal"));
        assert!(EvictionAlgorithm::Random.description().contains("random"));
    }

    #[test]
    fn test_round_trip_conversion() {
        // Test that name -> from_name -> name works correctly
        for &algorithm in EvictionAlgorithm::all() {
            let name = algorithm.name();
            let parsed = EvictionAlgorithm::from_name(name);
            assert_eq!(parsed, Some(algorithm), "Failed round-trip for {}", name);
        }
    }

    #[test]
    fn test_case_insensitive_parsing() {
        let test_cases = [
            ("LRU", EvictionAlgorithm::Lru),
            ("lru", EvictionAlgorithm::Lru),
            ("Lru", EvictionAlgorithm::Lru),
            ("S3FIFO", EvictionAlgorithm::S3Fifo),
            ("s3fifo", EvictionAlgorithm::S3Fifo),
            ("S3Fifo", EvictionAlgorithm::S3Fifo),
            ("SIEVE", EvictionAlgorithm::Sieve),
            ("sieve", EvictionAlgorithm::Sieve),
            ("Sieve", EvictionAlgorithm::Sieve),
        ];

        for (input, expected) in test_cases {
            assert_eq!(
                EvictionAlgorithm::from_name(input),
                Some(expected),
                "Failed to parse '{}' as {:?}",
                input,
                expected
            );
        }
    }

    #[test]
    fn test_algorithm_enum_completeness() {
        // Ensure we have a reasonable number of algorithms
        let all_algorithms = EvictionAlgorithm::all();
        assert!(all_algorithms.len() >= 40, "Expected at least 40 algorithms, got {}", all_algorithms.len());

        // Ensure no duplicates
        use std::collections::HashSet;
        let mut seen = HashSet::new();
        for &algorithm in all_algorithms {
            assert!(seen.insert(algorithm), "Duplicate algorithm found: {:?}", algorithm);
        }
    }
}
