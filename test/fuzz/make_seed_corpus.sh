#!/bin/bash
# Build the seed corpus for fuzz_traceReader from the sample traces in data/.
# Each seed is the selector byte fuzz_traceReader.c reads first, followed by
# the start of a real trace in that format.
#
# usage: test/fuzz/make_seed_corpus.sh <output dir>

set -euo pipefail

out=${1:?usage: $0 <output dir>}
data="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../data" && pwd)"
mkdir -p "${out}"

# cloudPhysicsIO.csv is version,time,op,size,lbn; the target reads time,id,size
csv_rows() { awk -F, 'NR > 1 && NR <= 200 { print $2 "," $5 "," $4 }' "${data}/cloudPhysicsIO.csv"; }

# 0x00: csv, comma, no header
{ printf '\000'; csv_rows; } >"${out}/csv"
# 0x01: csv, comma, with a header
{ printf '\001'; csv_rows; } >"${out}/csv_header"
# 0x02: plain text, one object id per line
{ printf '\002'; head -n 200 "${data}/cloudPhysicsIO.txt"; } >"${out}/txt"
# 0x03: oracleGeneral, 200 records of 24 bytes
{ printf '\003'; head -c 4800 "${data}/cloudPhysicsIO.oracleGeneral.bin"; } >"${out}/oracleGeneral"
# 0x05: lcs v1, the records after the 8 KiB header (the target writes its own)
# (head first: under pipefail, `tail | head` fails when head closes the pipe)
{ printf '\005'; head -c $((8192 + 4800)) "${data}/cloudPhysicsIO.vscsi.lcs" | tail -c 4800; } >"${out}/lcs_v1"
