use config::{load_access_records, Config, InnerConfig};
use draw::draw_lines;
use evict_policy::{EvictPolicy, FifoPolicy, LruPolicy};
use minisim::MiniSim;
use shards::ShardsFixedRate;
use std::thread;
use std::{error::Error, sync::Arc};
use tracing::{debug, info, Level};
use tracing_subscriber::FmtSubscriber;

mod config;
mod draw;
mod evict_policy;
mod minisim;
mod shards;

const NUM_CACHE_SIZE: u64 = 100;
type Key = u64;

fn init_logger() {
    // a builder for `FmtSubscriber`.
    let subscriber = FmtSubscriber::builder()
        // all spans/events with a level higher than TRACE (e.g, debug, info, warn, etc.)
        // will be written to stdout.
        .with_max_level(Level::TRACE)
        // completes the builder.
        .finish();

    tracing::subscriber::set_global_default(subscriber).expect("setting default subscriber failed");
}

#[derive(Debug, serde::Deserialize)]
struct AccessRecord {
    timestamp: u64,
    command: u8,
    key: u64,
    size: u32,
    ttl: u32,
}

struct SimulationResult {
    points: Vec<(f64, f64)>,
    label: String,
}

// Use multi thread to simulate
fn simulation<P: EvictPolicy>(
    access_records: Arc<Vec<AccessRecord>>,
    mut sim: MiniSim<P>,
    label: String,
) -> SimulationResult {
    let start = std::time::Instant::now();
    for access in access_records.iter() {
        sim.handle(access);
    }
    let points = sim.curve();
    let elapsed = start.elapsed();
    info!("{label} simulation took {elapsed:?}");
    SimulationResult { points, label }
}

fn simulate_all(access_records: Arc<Vec<AccessRecord>>, args: &InnerConfig) {
    let max_cache_size = args.cache_size;
    info!("Simulation policies: {:?}", args.policies);
    info!("Simple rate: {:?}", args.sample_rate);
    let handles = args
        .policies
        .iter()
        .map(|policy: &config::EvictionPolicy| {
            let access_records = Arc::clone(&access_records);
            let label = policy.to_string();
            let shards = ShardsFixedRate::create_shards(args.sample_rate);
            match policy {
                config::EvictionPolicy::LRU => {
                    let sim = MiniSim::<LruPolicy>::new(max_cache_size, shards);
                    thread::spawn(move || simulation(access_records, sim, label))
                }
                config::EvictionPolicy::FIFO => {
                    let sim = MiniSim::<FifoPolicy>::new(max_cache_size, shards);
                    thread::spawn(move || simulation(access_records, sim, label))
                }
            }
        })
        .collect::<Vec<_>>();

    let results = handles
        .into_iter()
        .map(|handle| handle.join().unwrap())
        .collect::<Vec<_>>();
    draw_lines(&results, args.output.clone());
}

fn main() -> Result<(), Box<dyn Error>> {
    init_logger();
    let config = Config::load();
    let access_records = load_access_records(&config);
    let config = InnerConfig::from(config);
    info!("Simulation config: {:?}", config);
    debug_assert!(access_records.len() > 0);
    debug!("Access records: length: {}", access_records.len());
    for record in access_records.iter().take(5) {
        debug!("{:?}", record);
    }
    let access_records = Arc::new(access_records);
    simulate_all(access_records.clone(), &config);
    debug!("Simulation completed successfully");
    Ok(())
}
