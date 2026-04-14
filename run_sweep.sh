#!/bin/bash
# Sweep miss ratio and retention ratio for the four GroupMerge variants.
# Usage: run_sweep.sh <trace_path> <trace_format> <out_csv> <size1> [size2 ...]
set -u
TRACE="$1"
FMT="$2"
OUT="$3"
shift 3
SIZES=("$@")

BIN="/proj/cache-PG0/jason/libcachesim_google/_build/bin/cachesim"
ALGOS=(groupmerge groupmergehead groupmergeadaptive groupmergeadaptive2)

echo "algo,size,miss_ratio,byte_miss_ratio,retain_ratio" > "$OUT"
for algo in "${ALGOS[@]}"; do
  for sz in "${SIZES[@]}"; do
    log=$("$BIN" "$TRACE" "$FMT" "$algo" "$sz" 2>&1)
    mr=$(echo "$log"  | grep -oE "miss ratio [0-9.]+"       | tail -1 | awk '{print $3}')
    bmr=$(echo "$log" | grep -oE "byte miss ratio [0-9.]+"  | tail -1 | awk '{print $4}')
    rr=$(echo "$log"  | grep -oE "byte ratio = [0-9.]+"     | tail -1 | awk '{print $4}')
    echo "$algo,$sz,$mr,$bmr,$rr" | tee -a "$OUT"
  done
done
