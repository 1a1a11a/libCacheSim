use crate::{evict_policy::EvictPolicy, shards::Shards, AccessRecord, Key, NUM_CACHE_SIZE};

pub struct MiniSim<P: EvictPolicy> {
    max_cache_size: u64,
    caches: Vec<P>,
    hits: Vec<u64>,
    access_count: u64,
    shards: Option<Box<dyn Shards>>,
}

fn get_caches<P: EvictPolicy>(
    max_cache_size: u64,
    num_caches: u64,
    shards: &Option<Box<dyn Shards>>,
) -> Vec<P> {
    (1..=num_caches)
        .map(|i| {
            // Represents the size of the cache.
            let mut cache_size = (i + 1) * (max_cache_size / num_caches as u64);
            // check cache_size > 100
            assert!(cache_size > 100);
            if let Some(shards) = shards.as_ref() {
                cache_size = shards.scale(cache_size);
            }
            P::new(cache_size)
        })
        .collect()
}

impl<P: EvictPolicy> MiniSim<P> {
    pub fn new(max_cache_size: u64, shards: Option<Box<dyn Shards>>) -> Self {
        let caches = get_caches(max_cache_size, NUM_CACHE_SIZE, &shards);

        MiniSim {
            max_cache_size,
            caches,
            hits: vec![0; NUM_CACHE_SIZE as usize],
            access_count: 0,
            shards,
        }
    }

    fn verify_shards(&mut self, key: Key) -> bool {
        if let Some(ref mut shards) = self.shards.as_mut() {
            if !shards.sample(&key) {
                return false;
            }
        }
        true
    }

    fn process(&mut self, access: &AccessRecord) {
        self.access_count += 1;

        for (i, cache) in self.caches.iter_mut().enumerate() {
            if let Some(_) = cache.get(access.key) {
                self.hits[i] += 1;
            } else {
                let size = if access.size == 0 { 1 } else { access.size };
                cache.put(access.key, size as u64);
            }
        }
    }

    pub fn handle(&mut self, access: &AccessRecord) {
        if !self.verify_shards(access.key) {
            return;
        }

        self.process(access);
    }

    pub fn curve(&self) -> Vec<(f64, f64)> {
        let mut points = Vec::new();
        for (i, hit) in self.hits.iter().enumerate() {
            let cache_size = (i + 1) * (self.max_cache_size as usize / NUM_CACHE_SIZE as usize);
            let mut miss_ratio = 1.0 - (*hit as f64 / self.access_count as f64);

            if let Some(shards) = self.shards.as_ref() {
                // If we are using a sampling rate, we need to adjust the miss ratio
                miss_ratio = ((miss_ratio * shards.get_sampled_count() as f64)
                    / shards.get_expected_count() as f64)
                    .clamp(0.0, 1.0)
            }

            points.push((cache_size as f64, miss_ratio));
        }
        return points;
    }
}
