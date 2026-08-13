#!/bin/bash
#
# Regression tests for the command-line tools.
#
# These cover argument handling and parameter reporting, which is not exercised
# by the C unit tests. Every case here crashed at some point: `-e print`
# dereferenced state that had not been built yet, options declared
# OPTION_ARG_OPTIONAL were handed a NULL argument in the space-separated form,
# and SLRU never validated n-seg.

set -uo pipefail

# Locate the binaries and the sample traces. ctest runs this from the build
# directory, but allow running it by hand from elsewhere too.
SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)

BIN_DIR=""
for candidate in "./bin" "../bin" "${SCRIPT_DIR}/../_build/bin" "${SCRIPT_DIR}/../build/bin"; do
	if [[ -x "${candidate}/cachesim" ]]; then
		BIN_DIR=$(cd "${candidate}" && pwd)
		break
	fi
done
if [[ -z "${BIN_DIR}" ]]; then
	echo "SKIP: cannot find the built binaries (looked for bin/cachesim)"
	exit 0
fi

DATA_DIR=""
for candidate in "./data" "../data" "../../data" "${SCRIPT_DIR}/../data"; do
	if [[ -f "${candidate}/cloudPhysicsIO.vscsi" ]]; then
		DATA_DIR=$(cd "${candidate}" && pwd)
		break
	fi
done
if [[ -z "${DATA_DIR}" ]]; then
	echo "SKIP: cannot find the sample traces (looked for data/cloudPhysicsIO.vscsi)"
	exit 0
fi

TRACE="${DATA_DIR}/cloudPhysicsIO.vscsi"
TRACE_ORACLE="${DATA_DIR}/cloudPhysicsIO.oracleGeneral.bin"
TRACE_CSV="${DATA_DIR}/cloudPhysicsIO.csv"
TRACE_TXT="${DATA_DIR}/cloudPhysicsIO.txt"

WORK_DIR=$(mktemp -d)
trap 'rm -rf "${WORK_DIR}"' EXIT
cd "${WORK_DIR}" || exit 1

N_PASS=0
N_FAIL=0

# Signals that mean a crash rather than a reported error. The project's ERROR()
# aborts (134), which is a deliberate exit, not a crash.
SIGSEGV_RC=139
SIGFPE_RC=136
SIGBUS_RC=138

_report() {
	if [[ $1 -eq 0 ]]; then
		N_PASS=$((N_PASS + 1))
	else
		N_FAIL=$((N_FAIL + 1))
		echo "  FAIL: $2"
	fi
}

# Command must exit 0.
expect_ok() {
	local desc=$1
	shift
	local out
	out=$("$@" 2>&1)
	local rc=$?
	if [[ ${rc} -eq 0 ]]; then
		_report 0 ""
	else
		_report 1 "${desc} (exit ${rc})"
		echo "${out}" | tail -3 | sed 's/^/        /'
	fi
}

# Command must exit 0 and its output must match a pattern.
expect_output() {
	local desc=$1 pattern=$2
	shift 2
	local out
	out=$("$@" 2>&1)
	local rc=$?
	if [[ ${rc} -ne 0 ]]; then
		_report 1 "${desc} (exit ${rc})"
		echo "${out}" | tail -3 | sed 's/^/        /'
	elif ! grep -qE "${pattern}" <<<"${out}"; then
		_report 1 "${desc} (output did not match /${pattern}/)"
		echo "${out}" | tail -3 | sed 's/^/        /'
	else
		_report 0 ""
	fi
}

# Invalid input must be rejected with a message, not a crash.
expect_clean_error() {
	local desc=$1
	shift
	local out
	out=$("$@" 2>&1)
	local rc=$?
	if [[ ${rc} -eq ${SIGSEGV_RC} || ${rc} -eq ${SIGFPE_RC} || ${rc} -eq ${SIGBUS_RC} ]]; then
		_report 1 "${desc} crashed with signal (exit ${rc})"
	elif [[ ${rc} -eq 0 ]]; then
		_report 1 "${desc} was accepted but should have been rejected"
	elif ! grep -qi "error" <<<"${out}"; then
		_report 1 "${desc} failed without an error message (exit ${rc})"
	else
		_report 0 ""
	fi
}

echo "running cachesim tests"

# Each supported sample trace format replays.
expect_output "cachesim vscsi" "miss ratio" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi lru 1gb
expect_output "cachesim txt" "miss ratio" \
	"${BIN_DIR}/cachesim" "${TRACE_TXT}" txt lru 1gb
expect_output "cachesim oracleGeneral" "miss ratio" \
	"${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral lru 1gb
expect_output "cachesim csv" "miss ratio" \
	"${BIN_DIR}/cachesim" "${TRACE_CSV}" csv lru 1gb \
	-t "time-col=2, obj-id-col=5, obj-size-col=4, obj-id-is-num=true"

# A numeric id column without obj-id-is-num is rejected, not silently hashed.
expect_clean_error "cachesim csv without obj-id-is-num" \
	"${BIN_DIR}/cachesim" "${TRACE_CSV}" csv lru 1gb \
	-t "time-col=2, obj-id-col=5, obj-size-col=4"

echo "running -e print tests"

# `-e print` runs before the cache is fully built, so the reporting path must
# not touch anything that is still uninitialized.
expect_output "slru -e print" "n-seg=4,seg-size=25:25:25:25" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi slru 1gb -e print
expect_output "slru -e n-seg=8,print" "n-seg=8" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi slru 1gb -e "n-seg=8,print"
expect_output "slru -e seg-size=1:2:3:4,print" "seg-size=9:19:29:39" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi slru 1gb -e "seg-size=1:2:3:4,print"
expect_output "slru -e print with auto sizing" "n-seg=" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi slru auto -e print
expect_output "qdlp -e print" "fifo-size-ratio=" \
	"${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral qdlp 1gb -e print
expect_output "s3fifod -e print" "fifo-size-ratio=" \
	"${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral s3fifod 1gb -e print

for algo in arc car clock clockpro lecar lru-prob beladysize fifo-merge s3fifo 2q; do
	expect_ok "${algo} -e print" \
		"${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral "${algo}" 1gb -e print
done

echo "running SLRU parameter validation tests"

# n-seg divides the cache size and the reported percentages, and seg-size fills
# a fixed-size array, so both are bounded.
expect_clean_error "slru n-seg=0" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi slru 1gb -e "n-seg=0"
expect_clean_error "slru n-seg=0 with print" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi slru 1gb -e "n-seg=0,print"
expect_clean_error "slru n-seg=-1" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi slru 1gb -e "n-seg=-1"
expect_clean_error "slru n-seg above the maximum" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi slru 1gb -e "n-seg=64"
expect_clean_error "slru empty seg-size" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi slru 1gb -e "seg-size=,print"
expect_clean_error "slru seg-size sums to zero" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi slru 1gb -e "seg-size=0:0"
expect_clean_error "slru too many segments" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi slru 1gb \
	-e "seg-size=1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1:1"
expect_clean_error "slru unknown parameter" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi slru 1gb -e "no-such-param=1"

echo "running option parsing tests"

# Options that take a value must accept the space-separated form. Declaring
# them OPTION_ARG_OPTIONAL made argp pass a NULL argument for "-o path".
if [[ -x "${BIN_DIR}/traceAnalyzer" ]]; then
	expect_ok "traceAnalyzer -o PATH" \
		"${BIN_DIR}/traceAnalyzer" -o out-spaced "${TRACE}" vscsi
	expect_ok "traceAnalyzer -oPATH" \
		"${BIN_DIR}/traceAnalyzer" -oout-attached "${TRACE}" vscsi
	expect_ok "traceAnalyzer --output=PATH" \
		"${BIN_DIR}/traceAnalyzer" --output=out-equals "${TRACE}" vscsi
	# A bare OPTION_ARG_OPTIONAL flag passes a NULL argument through is_true().
	expect_ok "traceAnalyzer --verbose" \
		"${BIN_DIR}/traceAnalyzer" --verbose "${TRACE}" vscsi
	expect_ok "traceAnalyzer --common" \
		"${BIN_DIR}/traceAnalyzer" --common -o out-common "${TRACE}" vscsi
	expect_ok "traceAnalyzer --num-req" \
		"${BIN_DIR}/traceAnalyzer" --num-req=10000 -o out-nreq "${TRACE}" vscsi
fi

if [[ -x "${BIN_DIR}/mrcProfiler" ]]; then
	expect_ok "mrcProfiler with space-separated options" \
		"${BIN_DIR}/mrcProfiler" "${TRACE}" vscsi \
		--algo LRU --profiler SHARDS --profiler-params FIX_RATE,0.01,42 --size 0.1,0.5,10
	expect_ok "mrcProfiler with = options" \
		"${BIN_DIR}/mrcProfiler" "${TRACE}" vscsi \
		--algo=LRU --profiler=SHARDS --profiler-params=FIX_RATE,0.01,42 --size=0.1,0.5,10
	expect_ok "mrcProfiler -o PATH" \
		"${BIN_DIR}/mrcProfiler" "${TRACE}" vscsi \
		--algo=LRU --profiler=SHARDS --profiler-params=FIX_RATE,0.01,42 --size=0.1,0.5,10 \
		-o mrc-out
	expect_ok "mrcProfiler --ignore-obj-size" \
		"${BIN_DIR}/mrcProfiler" "${TRACE}" vscsi \
		--algo=LRU --profiler=SHARDS --profiler-params=FIX_RATE,0.01,42 --size=0.1,0.5,10 \
		--ignore-obj-size
fi

echo
echo "${N_PASS} passed, ${N_FAIL} failed"
[[ ${N_FAIL} -eq 0 ]]
