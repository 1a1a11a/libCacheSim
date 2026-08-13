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

WORK_DIR=$(mktemp -d "${TMPDIR:-/tmp}/libcachesim_cli_test.XXXXXX")
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
	elif grep -qE "(AddressSanitizer|LeakSanitizer|ThreadSanitizer|MemorySanitizer|UndefinedBehaviorSanitizer|runtime error:)" <<<"${out}"; then
		# a sanitizer turns a crash into exit 1 with "ERROR: AddressSanitizer",
		# which the generic check below would read as a clean rejection. Test
		# this first so a crash cannot pass merely by printing the word error.
		_report 1 "${desc} tripped a sanitizer (exit ${rc})"
		grep -E "(Sanitizer|runtime error:)" <<<"${out}" | head -2 | sed 's/^/        /'
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

# String ids must be hashed, not run through strtoull. Getting this wrong
# collapses every object onto id 0, which shows up as an implausibly low miss
# ratio rather than an error. Four distinct objects in six requests, so a cache
# large enough to hold them all misses exactly four times.
cat >str-ids.csv <<'CSV'
time,id,size
1,alpha,100
2,beta,200
3,alpha,100
4,gamma,300
5,beta,200
6,delta,400
CSV
expect_output "cachesim hashes string object ids" "miss ratio 0\.6667" \
	"${BIN_DIR}/cachesim" str-ids.csv csv lru 1mb \
	-t "time-col=1,obj-id-col=2,obj-size-col=3,has-header=true"

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

# Sweep every algorithm the CLI registers rather than a hand-picked list: the
# crashes this covers were found in variants that a shorter list missed
# (s3fifov0, flashProb).
#
# Only these four sit behind a build flag (ENABLE_3L_CACHE, ENABLE_GLCACHE,
# ENABLE_LRB) and may legitimately be absent. Skipping on the "do not support
# algorithm" message alone would also skip a mandatory algorithm that had
# silently dropped out of the registry, which is precisely the regression this
# sweep exists to catch.
OPTIONAL_ALGOS=" 3LCache GLCache gl-cache lrb "

ALL_ALGOS="2q 3LCache CAR GLCache RandomLRU arc arcv0 cacheus clock clock2qplus
	clockpro fifo fifo-merge fifo-reinsertion fifomerge flashProb gdsf gl-cache
	lecar lecarv0 lfu lfucpp lfuda lhd lirs lrb lru lru-k lru-prob nop
	pluginCache qdlp random randomTwo s3-fifo s3-fifov0 s3fifo s3fifod s3fifov0
	sieve size slru slruv0 tinyLFU twoq wtinyLFU
	hyperbolic belady beladySize"

n_skipped=0
for algo in ${ALL_ALGOS}; do
	out=$("${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral "${algo}" 1gb -e print 2>&1)
	rc=$?
	if [[ ${rc} -ne 0 ]] && grep -qi "do not support algorithm" <<<"${out}" &&
		[[ ${OPTIONAL_ALGOS} == *" ${algo} "* ]]; then
		n_skipped=$((n_skipped + 1))
		continue
	fi
	if [[ ${rc} -eq 0 ]]; then
		_report 0 ""
	else
		_report 1 "${algo} -e print (exit ${rc})"
		echo "${out}" | tail -3 | sed 's/^/        /'
	fi
done
echo "  (${n_skipped} algorithms not compiled in, skipped)"

echo "running per-algorithm replay tests"

# Actually replay a trace with each algorithm, not just parse its parameters.
# Under the LeakSanitizer build CI uses, this is what catches allocations that
# init makes and free forgets — the `-e print` cases above exit early, so the
# cache is never torn down and a missing free stays invisible.
n_skipped=0
for algo in ${ALL_ALGOS}; do
	# pluginCache loads an eviction policy from an external .so that is not
	# built here; see doc/quickstart_plugin.md
	if [[ "${algo}" == "pluginCache" ]]; then
		n_skipped=$((n_skipped + 1))
		continue
	fi
	out=$("${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral "${algo}" 10mb \
		--num-req=20000 2>&1)
	rc=$?
	if [[ ${rc} -ne 0 ]] && grep -qi "do not support algorithm" <<<"${out}" &&
		[[ ${OPTIONAL_ALGOS} == *" ${algo} "* ]]; then
		n_skipped=$((n_skipped + 1))
		continue
	fi
	if [[ ${rc} -eq 0 ]]; then
		_report 0 ""
	else
		_report 1 "${algo} replay (exit ${rc})"
		echo "${out}" | tail -3 | sed 's/^/        /'
	fi
done
echo "  (${n_skipped} algorithms not compiled in, skipped)"

# Object metadata accounting reads from the sub-cache, which some algorithms
# only build partway through init.
for algo in wtinyLFU qdlp s3fifo slru lru; do
	expect_ok "${algo} replay with --consider-obj-metadata=true" \
		"${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral "${algo}" 10mb \
		--num-req=20000 --consider-obj-metadata=true
done

# WTinyLFU holds a window and a main cache whose per-object overheads differ
# (LRU and SLRU reserve 16 bytes, FIFO and Clock none), so each admission check
# has to use its own.
for main in FIFO LRU SLRU sieve ARC clock; do
	expect_ok "wtinyLFU main-cache=${main} with metadata" \
		"${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral wtinyLFU 10mb \
		--num-req=20000 -e "main-cache=${main}" --consider-obj-metadata=true
done

echo "running hash table sizing tests"

# --hashpower replaces a heuristic that keyed off the trace path. Sizing the
# table down is the point of it, so check the range is usable and validated.
for hp in 24 20 16 12; do
	expect_output "cachesim --hashpower=${hp}" "miss ratio" \
		"${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral lru 10mb \
		--num-req=20000 "--hashpower=${hp}"
done
for hp in 0 -1 40 99; do
	expect_clean_error "cachesim rejects --hashpower=${hp}" \
		"${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral lru 10mb \
		--num-req=20000 "--hashpower=${hp}"
done

# Composite policies size their sub-caches by subtracting from this. Without a
# floor the result reached zero, which cache_struct_init reads as "unset" and
# replaces with the full-size default — so asking for a small table allocated
# several large ones instead. slruv0 at hashpower 4 took 18 MB against 6 MB at 5.
for algo in slruv0 s3fifod cacheus lru; do
	for hp in 4 5 6 8; do
		expect_ok "${algo} at --hashpower=${hp}" \
			"${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral "${algo}" 10mb \
			--num-req=20000 "--hashpower=${hp}"
	done
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

	# A sample rate of 1 means no sampling. UINT64_MAX rounds up to 2^64 as a
	# double, so scaling by the rate before special-casing this converts a value
	# that does not fit back into uint64_t.
	for rate in 1 0.999 0.5 0.0001; do
		expect_ok "mrcProfiler SHARDS at sample rate ${rate}" \
			"${BIN_DIR}/mrcProfiler" "${TRACE}" vscsi \
			--algo=LRU --profiler=SHARDS --profiler-params="FIX_RATE,${rate},42" \
			--size=0.1,0.5,10
	done

	# Rates outside (0, 1] are rejected. nan needs the negated comparison, since
	# every ordinary comparison against it is false and it otherwise slipped
	# through to produce an all-1.0 curve and a zero exit.
	for rate in 0 -1 2 nan; do
		expect_clean_error "mrcProfiler rejects sample rate ${rate}" \
			"${BIN_DIR}/mrcProfiler" "${TRACE}" vscsi \
			--algo=LRU --profiler=SHARDS --profiler-params="FIX_RATE,${rate},42" \
			--size=0.1,0.5,10
	done

	# MINISIM looks its eviction algorithm up by name. It used to do that with
	# dlsym() against this executable, which cannot work when the constructors
	# sit in an unreferenced archive member, so every run aborted with
	# "undefined symbol: FIFO_init". Cover the non-LRU algorithms it exists for.
	# tinyLFU included: it is a cachesim alias, so name-based lookup has to
	# accept it too, or the promise the registry documents is not kept.
	for algo in FIFO ARC S3FIFO sieve twoq clock lfu tinyLFU; do
		expect_ok "mrcProfiler MINISIM with ${algo}" \
			"${BIN_DIR}/mrcProfiler" "${TRACE}" vscsi \
			--algo="${algo}" --profiler=MINISIM --profiler-params=FIX_RATE,0.01,4 \
			--size=0.1,0.5,10
	done

	expect_clean_error "mrcProfiler MINISIM with an unknown algorithm" \
		"${BIN_DIR}/mrcProfiler" "${TRACE}" vscsi \
		--algo=nosuchalgo --profiler=MINISIM --profiler-params=FIX_RATE,0.01,4 \
		--size=0.1,0.5,10

	# Above 0.5 MINISIM stops sampling and replays the whole trace, so the
	# miniature caches have to be full-sized. Scaling them by the requested rate
	# reported the miss ratios of smaller caches than were asked for.
	for rate in 0.6 0.75 1; do
		expect_ok "mrcProfiler MINISIM unsampled at rate ${rate}" \
			"${BIN_DIR}/mrcProfiler" "${TRACE_ORACLE}" oracleGeneral \
			--algo=LRU --profiler=MINISIM --profiler-params="FIX_RATE,${rate},4" \
			--size=100MB,500MB,3
	done

	# With sampling off MINISIM replays everything, so it should agree with a
	# straight cachesim run rather than approximate it.
	# first row is the 100MB point; 104857600B
	_minisim_unsampled=$("${BIN_DIR}/mrcProfiler" "${TRACE_ORACLE}" oracleGeneral \
		--algo=LRU --profiler=MINISIM --profiler-params=FIX_RATE,0.75,4 \
		--size=100MB,500MB,3 2>/dev/null | grep '^104857600B' | awk '{printf "%.4f", $2}')
	_cachesim_exact=$("${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral lru 100mb \
		2>/dev/null | tail -1 | grep -oE 'miss ratio [0-9.]+' | head -1 | awk '{printf "%.4f", $3}')
	if [[ "${_minisim_unsampled}" == "${_cachesim_exact}" ]]; then
		_report 0 ""
	else
		_report 1 "unsampled MINISIM (${_minisim_unsampled}) should match cachesim (${_cachesim_exact})"
	fi

	# belady and beladySize read next_access_vtime, which ordinary readers leave
	# unset, so on a non-oracle trace they must be refused rather than producing
	# a plausible-looking curve.
	for algo in belady beladySize; do
		expect_clean_error "mrcProfiler MINISIM rejects ${algo} on a vscsi trace" \
			"${BIN_DIR}/mrcProfiler" "${TRACE}" vscsi \
			--algo="${algo}" --profiler=MINISIM --profiler-params=FIX_RATE,0.01,4 \
			--size=0.1,0.5,10
		expect_ok "mrcProfiler MINISIM accepts ${algo} on an oracle trace" \
			"${BIN_DIR}/mrcProfiler" "${TRACE_ORACLE}" oracleGeneral \
			--algo="${algo}" --profiler=MINISIM --profiler-params=FIX_RATE,0.01,4 \
			--size=0.1,0.5,10
	done

	expect_ok "mrcProfiler SHARDS FIX_SIZE" \
		"${BIN_DIR}/mrcProfiler" "${TRACE}" vscsi \
		--algo=LRU --profiler=SHARDS --profiler-params=FIX_SIZE,2048,42 --size=100MB,1GB,10
fi

echo
echo "${N_PASS} passed, ${N_FAIL} failed"
[[ ${N_FAIL} -eq 0 ]]
