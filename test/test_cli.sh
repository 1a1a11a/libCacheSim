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

echo "running numeric option tests"

# Numeric arguments must be consumed in full and fit their destination type.
for option in num-thread num-req report-interval warmup-sec hashpower; do
	for value in "" abc 8junk 1.5 999999999999999999999999999999; do
		expect_clean_error "cachesim rejects --${option}=${value}" \
			"${BIN_DIR}/cachesim" "${TRACE}" vscsi lru 1mb \
			--num-req=10 --hashpower=8 "--${option}=${value}"
	done
done
for option in num-thread report-interval warmup-sec hashpower; do
	expect_clean_error "cachesim rejects --${option}=2147483648" \
		"${BIN_DIR}/cachesim" "${TRACE}" vscsi lru 1mb \
		--num-req=10 --hashpower=8 "--${option}=2147483648"
done
for option in num-thread num-req report-interval warmup-sec; do
	expect_clean_error "cachesim rejects --${option}=-2" \
		"${BIN_DIR}/cachesim" "${TRACE}" vscsi lru 1mb \
		--num-req=10 --hashpower=8 "--${option}=-2"
done
# The sampler stores the inverse ratio in an int; include its lower boundary.
for value in "" abc 0.5junk nan NaN inf -inf -0.1 1.1 1e999 1e-999 \
	1e-20 1e-308 4.656612875245796e-10; do
	expect_clean_error "cachesim rejects --sample-ratio=${value}" \
		"${BIN_DIR}/cachesim" "${TRACE}" vscsi lru 1mb \
		--num-req=10 --hashpower=8 "--sample-ratio=${value}"
done

# Preserve the full 64-bit request limit and the documented all-requests value.
for value in -1 4294967306 9223372036854775807; do
	expect_output "cachesim preserves --num-req=${value}" "113872 req" \
		"${BIN_DIR}/cachesim" "${TRACE}" vscsi lru 1mb \
		--hashpower=8 "--num-req=${value}"
done
expect_output "cachesim limits requests to 10" "10 req" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi lru 1mb \
	--hashpower=8 --num-req=10

# Explicit no-warmup must preserve requests at relative timestamp zero.
expect_output "cachesim accepts --warmup-sec=-1 without dropping requests" "10 req" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi lru 1mb \
	--num-req=10 --hashpower=8 --warmup-sec=-1

# Zero has historically disabled sampling; keep it alongside valid fractions.
for value in 0 0.5 5e-1 1 4.656612875245797e-10 4.656612875245798e-10; do
	expect_ok "cachesim accepts --sample-ratio=${value}" \
		"${BIN_DIR}/cachesim" "${TRACE}" vscsi lru 1mb \
		--num-req=10 --hashpower=8 "--sample-ratio=${value}"
done
expect_ok "cachesim keeps sampling state alive for parallel readers" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi lru 1mb,2mb \
	--num-req=100 --hashpower=8 --sample-ratio=0.5 --num-thread=2
for value in -1 0 1; do
	expect_ok "cachesim accepts --num-thread=${value}" \
		"${BIN_DIR}/cachesim" "${TRACE}" vscsi lru 1mb \
		--num-req=10 --hashpower=8 "--num-thread=${value}"
done
expect_ok "cachesim accepts zero warmup and report interval" \
	"${BIN_DIR}/cachesim" "${TRACE}" vscsi lru 1mb \
	--num-req=10 --hashpower=8 --warmup-sec=0 --report-interval=0

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

# --use-ttl. Object 1 has a TTL of 5 and is read again at t=10. Without TTLs
# that read hits (two misses in three requests); with TTLs it misses.
cat >ttl.csv <<'CSV'
time,id,size,ttl
1,1,100,5
2,2,100,0
10,1,100,0
CSV
TTL_FMT="time-col=1,obj-id-col=2,obj-size-col=3,ttl-col=4,has-header=true"
# A build without SUPPORT_TTL rejects --use-ttl=true; probe it on a trace
# without TTLs, since an expired read crashes TTL builds until #349 is fixed.
if "${BIN_DIR}/cachesim" str-ids.csv csv lru 1mb --use-ttl=true \
	-t "time-col=1,obj-id-col=2,obj-size-col=3,has-header=true" 2>&1 |
	grep -q "SUPPORT_TTL"; then
	expect_clean_error "cachesim --use-ttl=true without SUPPORT_TTL" \
		"${BIN_DIR}/cachesim" ttl.csv csv lru 1mb -t "${TTL_FMT}" --use-ttl=true
	expect_output "cachesim ignores TTLs without SUPPORT_TTL" "miss ratio 0\.6667" \
		"${BIN_DIR}/cachesim" ttl.csv csv lru 1mb -t "${TTL_FMT}"
else
	expect_output "cachesim --use-ttl=false drops the trace TTL" "miss ratio 0\.6667" \
		"${BIN_DIR}/cachesim" ttl.csv csv lru 1mb -t "${TTL_FMT}" --use-ttl=false
fi

echo "running synthetic reader tests"
for spec in Uniform_1000_10 Zipf_1000_10_0 Zipf_1000_10_1 \
	Zipf_1000_10_1.2_42 Zipf_1000_10_.5 Zipf_1000_10_1e-310; do
	expect_output "synthetic ${spec}" '1000 req, miss ratio 0\.0100' \
		"${BIN_DIR}/cachesim" "${spec}" syn lru 10 --hashpower=8 --num-thread=1
done
expect_output "synthetic cap of one" '1 req, miss ratio 1\.0000' \
	"${BIN_DIR}/cachesim" Uniform_100_10 syn lru 10 --num-req=1 --hashpower=8
expect_output "synthetic large source count with cap" '10 req' \
	"${BIN_DIR}/cachesim" Uniform_4294967306_10 syn lru 10 --num-req=10 --hashpower=8
expect_output "synthetic observed working set" 'working set size: 1 object 1 byte' \
	"${BIN_DIR}/cachesim" Uniform_1_1000 syn lru 0.5 --hashpower=8
expect_ok "synthetic parallel caches" \
	"${BIN_DIR}/cachesim" Zipf_1000_100_0.9_42 syn lru,fifo 10,50 --num-thread=2 --hashpower=8
expect_ok "synthetic spatial sampling" \
	"${BIN_DIR}/cachesim" Zipf_1000_100_0.9 syn lru 10 --sample-ratio=0.5 --hashpower=8
for spec in '' x Uniform Uniform_0_10 Uniform_10_0 Uniform_-1_10 \
	Uniform_1x_10 Uniform_1_10x Uniform_1_2147483648 \
	Uniform_9223372036854775808_1 Uniform_1_1_18446744073709551616 \
	Uniform_1_1_-1 Uniform_1_1_ Uniform_1_1_2_extra \
	Zipf_10_10 Zipf_10_10_ Zipf_10_10_-1 Zipf_10_10_nan \
	Zipf_10_10_inf Zipf_10_10_1junk Zipf_10_10_0x1p0 \
	Zipf_10_10_1e999 Zipf_10_10_1e-999 Zipf_10_10_1_2_extra; do
	# Require exit 1 explicitly: ERROR()/abort must not count as validation.
	out=$("${BIN_DIR}/cachesim" "${spec}" syn lru 10 --hashpower=8 2>&1)
	rc=$?
	if [[ ${rc} -eq 1 ]] && grep -q 'error: invalid synthetic trace' <<<"${out}" &&
		! grep -qE 'Sanitizer|runtime error:' <<<"${out}"; then
		_report 0 ""
	else
		_report 1 "synthetic rejects '${spec}' cleanly (exit ${rc})"
	fi
done
for param in time-col=1 obj-size-col=1 has-header=true obj-id-is-num=false; do
	expect_clean_error "synthetic rejects ${param}" \
		"${BIN_DIR}/cachesim" Uniform_100_10 syn lru 10 -t "${param}" --hashpower=8
done

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
expect_output "s3fifo -e print" "small-size-ratio=" \
	"${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral s3fifo 1gb -e print

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
	lecar lecarv0 lfu lfucpp lfuda lhd lirs lrb lru lru-k lru-prob merlin mq
	multiqueue nop
	pluginCache qdlp random randomTwo s3-fifo s3-fifov0 s3fifo s3fifov0
	sieve size slru slruv0 tinyLFU twoq wtinyLFU
	hyperbolic belady beladySize"

# Algorithms with no tunable parameters at all: `-e print` is accepted, silently
# ignored, and the run replays the whole trace instead of reporting anything.
# Everything else has to produce a parameter report, so an algorithm newly added
# to ALL_ALGOS lands in the strict branch by default and fails loudly rather
# than passing without the print path having been exercised.
#
# arc and arcv0 used to be here: they define a *_parse_params with a working
# `print` branch that their inits never called. Those inits parse their
# parameters now, so both report like everything else and belong in the strict
# branch.
NO_PRINT_ALGOS=" belady cacheus fifo gdsf lecarv0 lfu lfucpp lfuda lhd lirs"
NO_PRINT_ALGOS="${NO_PRINT_ALGOS} lru nop random randomTwo sieve size "

n_skipped=0
for algo in ${ALL_ALGOS}; do
	out=$("${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral "${algo}" 1gb -e print 2>&1)
	rc=$?
	if [[ ${rc} -ne 0 ]] && grep -qi "do not support algorithm" <<<"${out}" &&
		[[ ${OPTIONAL_ALGOS} == *" ${algo} "* ]]; then
		n_skipped=$((n_skipped + 1))
		continue
	fi
	if [[ ${rc} -ne 0 ]]; then
		_report 1 "${algo} -e print (exit ${rc})"
		echo "${out}" | tail -3 | sed 's/^/        /'
		continue
	fi
	if [[ ${NO_PRINT_ALGOS} == *" ${algo} "* ]]; then
		_report 0 ""
		continue
	fi
	# Drop the colour codes and the logger's own lines before looking for the
	# report. The INFO banner echoes back "eviction-params: print", so a looser
	# search matches every run -- including the ones that never print -- and the
	# assertion passes without the print path having run at all.
	#
	# The escape is a literal one through ANSI-C quoting rather than \x1b, which
	# only GNU sed understands: BSD sed on the macOS job reads \x as a plain x,
	# matches nothing, and strips no colour at all. Nothing writes to stderr on
	# this path today -- the printing algorithms exit inside create_cache(),
	# before print_parsed_args() logs anything -- so this guard is dormant on
	# every platform. Keep it correct anyway, so it still works the day an
	# algorithm warns while initialising.
	_printed=$(sed $'s/\033\\[[0-9;]*m//g' <<<"${out}" | grep -v '^\[')
	if grep -qE '^([A-Za-z0-9_.-]+ )?(current |default )?param(eter)?s?: ' <<<"${_printed}"; then
		_report 0 ""
	else
		_report 1 "${algo} -e print exited 0 without reporting parameters"
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
for algo in wtinyLFU qdlp s3fifo slru lru merlin; do
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
for algo in slruv0 s3fifo cacheus lru; do
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

echo "running Merlin parameter validation tests"
expect_output "merlin -e print" "epoch-update=32,sketch-scale=1" \
  "${BIN_DIR}/cachesim" "${TRACE}" vscsi merlin 1gb -e print
expect_output "merlin custom parameters" "epoch-update=4,sketch-scale=0.01" \
  "${BIN_DIR}/cachesim" "${TRACE}" vscsi merlin 1gb -e "epoch-update=4,sketch-scale=0.01,print"
for params in "filter-size-ratio=0" "filter-size-ratio=nan" \
    "staging-size-ratio=-1" "filter-size-ratio=0.8,staging-size-ratio=0.3" \
    "ghost-size-ratio=-1" "epoch-update=0" "epoch-update=1.5" \
    "epoch-update=2147483648" "sketch-scale=inf" "sketch-scale=0" \
    "sketch-scale=" "epoch-update" "no-such-param=1" "epoch-update=3x"; do
  expect_clean_error "merlin rejects ${params}" \
    "${BIN_DIR}/cachesim" "${TRACE}" vscsi merlin 1mb -e "${params}"
done
expect_output "merlin replay with custom parameters" "miss ratio" \
  "${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral merlin 1mb \
  --num-req=20000 --hashpower=12 -e "epoch-update=4,sketch-scale=0.001"

expect_clean_error "merlin rejects unsupported prefetching" \
  "${BIN_DIR}/cachesim" "${TRACE_ORACLE}" oracleGeneral merlin 1mb \
  --num-req=10 --hashpower=4 -e "sketch-scale=0.001" --prefetch=OBL

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

	# MINISIM needs a thread count. Omitting it used to leave thread_num at 0
	# and skip the validation that only runs when the field is present, so the
	# run aborted inside the thread pool with "cannot push data into
	# thread_pool" -- no mention of the parameter that was missing, and no
	# error message for a caller to act on.
	expect_clean_error "mrcProfiler MINISIM without a thread count" \
		"${BIN_DIR}/mrcProfiler" "${TRACE}" vscsi \
		--algo=LRU --profiler=MINISIM --profiler-params=FIX_RATE,0.01 \
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
	# both sides must actually have been extracted: if the output formats change
	# and neither grep matches, "" == "" would record this assertion as passed
	# without either miss ratio having been observed.
	if [[ -z "${_minisim_unsampled}" || -z "${_cachesim_exact}" ]]; then
		_report 1 "could not read a miss ratio to compare (minisim='${_minisim_unsampled}' cachesim='${_cachesim_exact}')"
	elif [[ "${_minisim_unsampled}" == "${_cachesim_exact}" ]]; then
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
