"""
This script benchmarks the throughput and other performance metrics of different
caching algorithms using `perf stat`.

It can either generate synthetic Zipfian traces or use existing trace files.
For each combination of trace, algorithm, and cache size, it runs `cachesim`
under `perf stat`, parses the performance data, and aggregates the results
into a CSV file.

Example Usage:
    # Using a pre-existing trace
    python3 scripts/benchmark_throughput.py \\
        --tracepath ../data/twitter_cluster52.csv.zst \\
        --algos=lru,s3fifo \\
        --sizes=0.1

    # Generating synthetic traces and running on them
    python3 scripts/benchmark_throughput.py \\
        --num-objects=1000000 \\
        --num-requests=10000000 \\
        --alpha=0.8,1.0 \\
        --algos=lru,s3fifo \\
        --sizes=0.1
"""

import subprocess
import logging
import argparse
from typing import Dict
from utils.setup_utils import setup, CACHESIM_PATH
import re
import csv
import os
import multiprocessing
import pandas as pd

logger = logging.getLogger("cache_sim_monitor")
logging.basicConfig(level=logging.INFO, format="%(asctime)s - %(levelname)s - %(message)s")

def generate_trace(args: Tuple[int, int, float, str]) -> Optional[str]:
    """
    Generates a single synthetic trace file using data_gen.py.

    This function is designed to be called by a multiprocessing pool.

    Args:
        args: A tuple containing (num_objects, num_requests, alpha, output_dir).

    Returns:
        The path to the generated trace file, or None if generation failed.
    """
    m, n, a, output_dir = args
    trace_filename = f"{output_dir}/zipf_{a}_{m}_{n}.oracleGeneral"
    
    if os.path.exists(trace_filename):
        logger.info(f"Trace {trace_filename} already exists. Skipping.")
        return trace_filename
    
    cmd = [
        "python3", "data_gen.py",
        "-m", str(m),
        "-n", str(n),
        "--alpha", str(a),
        "--bin-output", trace_filename
    ]
    cmmd = " ".join(cmd)
    logger.info(f"Generating trace: {trace_filename}, Command: {cmmd}")
    process = subprocess.run(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    
    if process.returncode != 0:
        logger.warning(f"Failed to generate trace {trace_filename}")
        logger.warning(process.stderr.decode("utf-8"))
        return None
    return trace_filename


def generate_synthetic_traces(num_objects: str, num_requests: str, alpha: str) -> List[str]:
    """
    Generates multiple synthetic trace files in parallel.

    Args:
        num_objects: Comma-separated string of the number of unique objects.
        num_requests: Comma-separated string of the total number of requests.
        alpha: Comma-separated string of Zipfian distribution parameters.

    Returns:
        A list of paths to the generated trace files.
    """
    num_objects = [int(x) for x in num_objects.split(",")]
    num_requests = [int(x) for x in num_requests.split(",")]
    alpha = [float(x) for x in alpha.split(",")]

    output_dir = "../data/synthetic_traces"
    os.makedirs(output_dir, exist_ok=True)

    args_list = [(m, n, a, output_dir) for m in num_objects for n in num_requests for a in alpha]

    logger.info(f"Generating {len(args_list)} traces with {min(len(args_list), os.cpu_count())} processes")
    with multiprocessing.Pool(processes=min(len(args_list), os.cpu_count())) as pool:
        traces = pool.map(generate_trace, args_list)

    traces = [t for t in traces if t is not None]

    logger.info(f"Generated {len(traces)} traces.")
    return traces
    
    
def parse_perf_stat(perf_stat_output: str) -> Dict[str, float]:
    """
    Parses the output of `perf stat` to extract performance metrics.

    Args:
        perf_stat_output: The stderr string from the `perf stat` command.

    Returns:
        A dictionary mapping metric names to their values.
    """
    metrics_regex = {
        "cpu_utilization": r"([\d\.]+)\s+CPUs utilized",
        "task_clock_msec": r"([\d\.]+)\s+msec task-clock",
        "throughput": r"throughput\s+([\d\.]+)\s+MQPS",
        "context_switches": r"([\d\.]+)\s+context-switches",
        "cpu_migrations": r"([\d\.]+)\s+cpu-migrations",
        "cpu_cycles": r"([\d\.]+)\s+cycles",
        "instructions": r"([\d\.]+)\s+instructions",
        "ipc": r"([\d\.]+)\s+insn per cycle",
        "elapsed_time_sec": r"([\d\.]+)\s+seconds time elapsed",
        "user_time_sec": r"([\d\.]+)\s+seconds user",
        "sys_time_sec": r"([\d\.]+)\s+seconds sys"
    }
    
    perf_data = {}

    for key, regex in metrics_regex.items():
        match = re.search(regex, perf_stat_output)
        if match:
            try:
                perf_data[key] = float(match.group(2) if len(match.groups()) > 1 else match.group(1))
            except ValueError:
                logger.warning(f"Failed to convert {key} to float")
                pass 

    return perf_data
    
def run_cachesim(trace: str, algo: str, cache_size: str, ignore_obj_size: bool, num_thread: int, trace_format: str, trace_format_params: str) -> Dict[str, float]:
    """
    Runs a single cachesim instance under `perf stat` and captures the output.

    Args:
        trace: Path to the trace file.
        algo: The caching algorithm to benchmark.
        cache_size: The cache size to use.
        ignore_obj_size: Whether to treat all objects as size 1.
        num_thread: Number of threads for the simulation.
        trace_format: The format of the trace file.
        trace_format_params: Additional parameters for the trace format.

    Returns:
        A dictionary of performance metrics from `parse_perf_stat`.
    """
    logger.info(f"Running perf with trace={trace}, algo={algo}, size={cache_size}")

    run_args = [
        "sudo", "perf", "stat", "-d",  
        CACHESIM_PATH,
        trace,
        trace_format, 
        algo,
        cache_size,
        "--ignore-obj-size", "1" if ignore_obj_size else "0",
        "--num-thread", str(num_thread),
    ]
    
    if trace_format_params:
        run_args.append("--trace-type-params")
        run_args.append(trace_format_params)

    
    p = subprocess.run(run_args, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        
    stdout_str = p.stdout.decode("utf-8")
    perf_json = parse_perf_stat(stdout_str)
    
    if p.returncode != 0 or "Segmentation fault" in stdout_str:
        logger.warning("CACHESIM may have crashed with segfault")
        perf_json = {}
            
    return perf_json


def generate_summary(results: List[Dict]):
    """
    Generates CSV summary files from the collected performance data.

    Creates two files:
    - result/throughput_log.csv: Contains the raw results for every run.
    - result/throughput_avg.csv: Contains results averaged across all traces
      for each algorithm and cache size combination.

    Args:
        results: A list of dictionaries, where each dictionary holds the
                 performance data for a single run.
    """
    summary_file = "result/throughput_log.csv"
    os.makedirs("result", exist_ok=True)
    
    df = pd.DataFrame(results)
    # algo and cache size should be 1st and 2nd columns
    column_order = ['algo', 'cache_size'] + [a for a in df.columns if a not in ['algo', 'cache_size']]
    df = df.reindex(columns=column_order)
    df.to_csv(summary_file, index=False)
    logger.info(f"Summary saved to {summary_file}")
    
    logger.info("Averaging out across all trace")
    df = df.drop(columns=['trace'])
    avg_df = df.groupby(['algo', 'cache_size']).mean().reset_index()
    avg_df.to_csv("result/throughput_avg.csv", index=False)

    logger.info(f"Average summary saved to result/throughput_avg.csv")
    
        
def main():
    """
    Main function to parse command-line arguments and orchestrate the benchmark.
    """
    default_args = {
        "algos": "fifo,lfu,lhd,GLCache",
        "sizes": "0.1",
        "num_objects": "100,1000",
        "num_requests": "10000",
        "alpha": "0.1, 0.2",
    }
    parser = argparse.ArgumentParser(
        description="Run cachesim with CPU monitoring"
    )
    parser.add_argument("--tracepath", type=str, required=False, help="Trace file path")
    parser.add_argument(
        "--num-objects", type=str, default=default_args["num_objects"],
        help="Number of objects"
    )
    parser.add_argument(
        "--num-requests", type=str, default=default_args["num_requests"],
        help="Number of requests"
    )
    parser.add_argument(
        "--alpha", type=str, default=default_args["alpha"],
        help="Zipf parameter"
    )
    parser.add_argument(
        "--algos", type=str,
        default=default_args["algos"],
        help="The algorithms to run, separated by comma"
    )
    parser.add_argument(
        "--sizes", type=str,
        default=default_args["sizes"],
        help="The cache sizes to run, separated by comma"
    )
    parser.add_argument("--trace-format", type=str, default="oracleGeneral")
    parser.add_argument(
        "--trace-format-params", type=str,
        default="", help="Used by CSV trace"
    )
    parser.add_argument(
        "--ignore-obj-size", action="store_true",
        default=False, help="Ignore object size"
    )
    parser.add_argument(
        "--num-thread", type=int, default=-1,
        help="Number of threads to use"
    )
    parser.add_argument("--name", type=str, default="")
    parser.add_argument(
        "--verbose", action="store_true", default=False,
        help="Enable verbose logging"
    )

    args = parser.parse_args()

    if args.verbose:
        logger.setLevel(logging.DEBUG)
    else:
        logger.setLevel(logging.INFO)

    # Parse arguments
    traces = args.tracepath.split(",") if args.tracepath else generate_synthetic_traces(
        num_objects=args.num_objects,
        num_requests=args.num_requests,
        alpha=args.alpha
    )
    algos = args.algos.split(",")
    cache_sizes = args.sizes.split(",")
    
    results = []
    # Run perf on cachesim over each combination of trace, algo, cache_size
    for trace in traces:
        for algo in algos:
            for cache_size in cache_sizes:
                result_json = run_cachesim(
                    trace=trace,
                    algo=algo,
                    cache_size=cache_size,
                    ignore_obj_size=args.ignore_obj_size,
                    num_thread=args.num_thread,
                    trace_format=args.trace_format,
                    trace_format_params=args.trace_format_params
                )
                result_json['algo'] = algo
                result_json['cache_size'] = cache_size
                result_json['trace'] = trace
                results.append(result_json)

    generate_summary(results)
    

if __name__ == "__main__":
    main()