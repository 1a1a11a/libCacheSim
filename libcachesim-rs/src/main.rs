use libcachesim_rs::bindings::*;
use libcachesim_rs::wrapper::*;

fn main() {
    // Open the trace file
    println!("main test file");

    let mut init_params_csv = Reader::default_reader_init_params();
    init_params_csv.delimiter = ',' as i8; // b',' as std::os::raw::c_char,
    init_params_csv.time_field = 2;
    init_params_csv.obj_id_field = 6;
    init_params_csv.obj_size_field = 4;
    init_params_csv.has_header = false;

    let mut reader = match Reader::open_trace("../data/trace.csv", trace_type_e_CSV_TRACE, &init_params_csv) {
        Some(r) => r,
        None => {
            eprintln!("Failed to open trace.");
            return;
        }
    };

    // Create a request container
    let req = match Request::new_request() {
        Some(r) => r,
        None => {
            eprintln!("Failed to create request.");
            return;
        }
    };

    // Define cache parameters and create the cache
    let cache_params = common_cache_params_t {
        cache_size: 1024 * 1024,
        ..Default::default()
    };
    let cache = match Cache::LRU_init(cache_params, "") {
        Some(c) => c,
        None => {
            eprintln!("Failed to initialize cache.");
            return;
        }
    };

    // Counters
    let mut n_req = 0;
    let mut n_miss = 0;

    // Loop through the trace
    while reader.read_one_req(&req) == 0 {
        if !cache.get(&req) {
            n_miss += 1;
        }
        n_req += 1;
    }

    // Print the miss ratio
    if n_req > 0 {
        println!("miss ratio: {:.4}", n_miss as f64 / n_req as f64);
    } else {
        println!("No requests were processed.");
    }

    // Clean up happens automatically due to Drop implementations
}
