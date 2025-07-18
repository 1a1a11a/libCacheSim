use libcachesim::{Cache, CacheConfig, EvictionAlgorithm, CacheKey};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("libCacheSim Rust Bindings - Comprehensive Examples");
    
    let mut cache = Cache::new(
        EvictionAlgorithm::Lru,
        CacheConfig::default()
    )?;
    
    cache.insert(CacheKey::Numeric(1), 1024)?;
    let hit = cache.get(&CacheKey::Numeric(1))?;
    println!("Cache hit: {}", hit);
    
    let stats = cache.stats();
    println!("Hit rate: {:.2}%", stats.hit_rate_percent());
    
    println!("✅ Comprehensive examples completed successfully!");
    Ok(())
}
