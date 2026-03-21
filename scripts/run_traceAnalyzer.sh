#!/bin/bash

set -euo pipefail

function help() {
    echo "Usage: $0 <tracepath> <trace_format> [<trace_format_parameters>]"
}

if [ $# -lt 2 ]; then
    help;
    exit 1;
fi

CURR_DIR=$(cd "$(dirname "$0")"; pwd)
REPO_DIR=$(cd "${CURR_DIR}/.."; pwd)
TRACE_ANALYZER="${REPO_DIR}/_build/bin/traceAnalyzer"

if [ ! -x "${TRACE_ANALYZER}" ]; then
    echo "traceAnalyzer binary not found at ${TRACE_ANALYZER}; build the project first" >&2
    exit 1
fi

tracepath=$1
trace_format=$2
shift 2

analyzer_args=("${tracepath}" "${trace_format}" "--common")
if [ $# -gt 0 ]; then
    analyzer_args+=("--trace-type-params" "$*")
fi

"${TRACE_ANALYZER}" "${analyzer_args[@]}"

dataname=$(basename "${tracepath}")
python3 "${CURR_DIR}/traceAnalysis/access_pattern.py" "${dataname}.accessRtime"
python3 "${CURR_DIR}/traceAnalysis/access_pattern.py" "${dataname}.accessVtime"
python3 "${CURR_DIR}/traceAnalysis/req_rate.py" "${dataname}.reqRate_w300"
python3 "${CURR_DIR}/traceAnalysis/size.py" "${dataname}.size"
python3 "${CURR_DIR}/traceAnalysis/reuse.py" "${dataname}.reuse"
python3 "${CURR_DIR}/traceAnalysis/popularity.py" "${dataname}.popularity"
# python3 ${CURR_DIR}/traceAnalysis/requestAge.py ${dataname}.requestAge
# python3 ${CURR_DIR}/traceAnalysis/size_heatmap.py ${dataname}.sizeWindow_w300
# python3 ${CURR_DIR}/traceAnalysis/futureReuse.py ${dataname}.access

# python3 ${CURR_DIR}/traceAnalysis/popularity_decay.py ${dataname}.popularityDecay_w300_obj
# python3 ${CURR_DIR}/traceAnalysis/reuse_heatmap.py ${dataname}.reuseWindow_w300
