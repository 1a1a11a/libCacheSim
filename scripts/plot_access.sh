#!/bin/bash

set -euo pipefail


BIN=/proj/redundancy-PG0/jason/cacheTraceAnalysis/c/_build/bin/analyze
SCRIPT_DIR=/proj/redundancy-PG0/jason/cacheTraceAnalysis/c/scripts/

data=/disk/io_traces.ns23.oracleGeneral.zst
data=/disk/io_traces.ns435.oracleGeneral.zst
data=$1

dataname=$(basename ${data})
dataname=$(echo ${dataname/.zst/})
dataname=$(echo ${dataname/.oracleGeneral/})
dataname=$(echo ${dataname/.sample10/})
dataname=$(echo ${dataname/.bin/})


mkdir data fig/ 2>/dev/null || true
${BIN} --task accessPattern -i ${data} -t oracleGeneral -o data/${dataname} &
${BIN} --task reuse -i ${data} -t oracleGeneral -o data/${dataname} &
wait
python3 ${SCRIPT_DIR}/accessPattern.py data/${dataname}.accessRtime &
python3 ${SCRIPT_DIR}/accessPattern.py data/${dataname}.accessVtime &

python3 ${SCRIPT_DIR}/reuse.py --datapath data/${dataname}.reuse &
