## Build Cache MRC

Cache Miss Ratio Curves (MRC) is a tool to generate MRCs for a given cache configuration. The tool is implemented in Rust and can be built using Cargo.

To enhance performance, we implemented the [SHARDS](https://www.usenix.org/conference/atc17/technical-sessions/presentation/waldspurger) idea, which applies sampling techniques to both the data stream and cache size.

Make sure you have installed Rust and Cargo. If not, please follow the instructions [here](https://www.rust-lang.org/tools/install).
```bash
cd ./libCacheSim/bin/cachemrc 
cargo build
```
## Running the MRC Generator
You can run the MRC Generator using either a configuration file or command-line arguments.
### Using a Configuration File
```bash
cargo run -r -- --config-file config.toml
```
### Using Command Line Arguments
```bash
cargo run -r -- --trace "../../../data/twitter_cluster52.csv" --output "./mrc.png" --policies lru,fifo --sample-rate 0.01 --cache-size=10mb --timestamp 0 --key 1 --size 2
```
## Options
--trace: Path to the input trace file
--output: Path for the output MRC graph
--policies: Comma-separated list of cache policies to evaluate (Currently only LRU and FIFO are supported)
--sample-rate: Sampling rate
--cache-size: Maximum cache size to simulate
Followings are the column indices in the trace file.
-1: means use the default value(eg, 0 for timestamp, 1 for size)
>0: means the column index in the trace file
--timestamp: Column index for timestamp in the trace file
--key: Column index for the cache key in the trace file
--size: Column index for the object size in the trace file

## Roadmap
- [x] Implement the basic MRC generation tool
- [x] Incorporate sampling method based on SHARDS
- [ ] Expand support for additional cache policies (e.g., LFU, TwoQueue)
- [ ] Add compatibility with more trace file formats (e.g., bin, vscsi)
