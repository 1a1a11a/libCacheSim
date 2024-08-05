use fasthash::murmur3;

use crate::Key;

const MODULUS: u64 = 1000;
fn hash(key: Key) -> u128 {
    murmur3::hash128(key.to_le_bytes())
}

pub trait Shards: Send {
    fn get_global_t(&self) -> u64;
    fn get_sampled_count(&self) -> u64;

    fn get_expected_count(&self) -> u64;

    fn get_rate(&self) -> f64 {
        self.get_global_t() as f64 / MODULUS as f64
    }

    fn sample(&mut self, access: &Key) -> bool;

    fn sample_key(&self, key: Key) -> Option<u64> {
        let t = (hash(key) % MODULUS as u128) as u64;

        match t < self.get_global_t() {
            true => Some(t),
            false => None,
        }
    }

    fn scale(&self, size: u64) -> u64 {
        (size as f64 * self.get_rate()) as u64
    }
}

pub struct ShardsFixedRate {
    global_t: u64,
    sampled_count: u64,
    total_count: u64,
}

impl ShardsFixedRate {
    pub fn new(global_t: u64) -> Self {
        ShardsFixedRate {
            global_t,
            sampled_count: 0,
            total_count: 0,
        }
    }

    pub fn create_shards(simple_rate: Option<f64>) -> Option<Box<dyn Shards>> {
        match simple_rate {
            Some(rate) => Some(Box::new(ShardsFixedRate::new(
                (rate * MODULUS as f64) as u64,
            ))),
            None => None,
        }
    }
}

impl Shards for ShardsFixedRate {
    fn get_global_t(&self) -> u64 {
        self.global_t
    }

    fn get_sampled_count(&self) -> u64 {
        self.sampled_count
    }

    fn get_expected_count(&self) -> u64 {
        (self.get_rate() * self.total_count as f64) as u64
    }

    fn sample(&mut self, access: &Key) -> bool {
        self.total_count += 1;

        if self.sample_key(*access).is_none() {
            return false;
        }

        self.sampled_count += 1;

        true
    }
}
