#!/usr/bin/env python3
"""Issue #361: filtering and oracle timestamps must describe the same stream."""

import functools
from pathlib import Path
import random
import re
import resource
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest


RECORD = struct.Struct("<IQIq")
BIN = Path(sys.argv.pop(1)).resolve()
WITH_ZSTD = "--with-zstd" in sys.argv
if WITH_ZSTD:
    sys.argv.remove("--with-zstd")


def oracle_records(rows):
    """Independent reference: rows are (timestamp, object id, size)."""
    future = {}
    result = []
    for index in range(len(rows) - 1, -1, -1):
        timestamp, oid, size = rows[index]
        result.append((timestamp, oid, size, future.get(oid, -1)))
        future[oid] = index + 1
    return list(reversed(result))


def optimal_misses(ids, capacity):
    """Exhaustive mandatory-insertion paging optimum, independent of MIN."""
    @functools.lru_cache(None)
    def solve(index, resident):
        if index == len(ids):
            return 0
        oid = ids[index]
        objects = set(resident)
        if oid in objects:
            return solve(index + 1, resident)
        if len(objects) < capacity:
            return 1 + solve(index + 1, tuple(sorted(objects | {oid})))
        return 1 + min(
            solve(index + 1, tuple(sorted((objects - {victim}) | {oid})))
            for victim in objects
        )
    return solve(0, ())


class OracleTraceTest(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory(prefix="oracle-test-")
        self.work = Path(self.directory.name)
        self.addCleanup(self.directory.cleanup)

    def run_tool(self, name, *args, ok=True):
        result = subprocess.run(
            [str(BIN / name), *map(str, args)], cwd=self.work,
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
            timeout=30,
        )
        if ok:
            self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        else:
            self.assertEqual(result.returncode, 1, result.stdout + result.stderr)
        self.assertNotRegex(result.stderr, r"(?:Address|Leak|UndefinedBehavior)Sanitizer")
        return result

    def write_trace(self, rows, name="input.oracleGeneral.bin"):
        path = self.work / name
        path.write_bytes(b"".join(RECORD.pack(*r) for r in oracle_records(rows)))
        return path

    def convert(self, path, fmt="oracleGeneral", *options):
        output = self.work / ("repaired." + fmt)
        self.run_tool("traceConv", path, "oracleGeneral", "-o", output,
                      "--output-format=" + fmt, "--output-txt=1", *options)
        return output

    def assert_oracle(self, output, rows, fmt="oracleGeneral"):
        expected = oracle_records(rows)
        if fmt == "oracleGeneral":
            actual = list(RECORD.iter_unpack(output.read_bytes()))
        else:
            header = output.read_bytes()[:8192]
            # LCS: two uint64 header fields, followed by the stat structure.
            n_req = struct.unpack_from("<q", header, 16 + 8)[0]
            start, end = struct.unpack_from("<qq", header, 16 + 5 * 8)
            self.assertEqual(n_req, len(rows))
            self.assertEqual((start, end), (rows[0][0], rows[-1][0]))
            # LCS writes the same first four fields in its text sidecar.
            actual = [tuple(map(int, line.split(",")[:4]))
                      for line in Path(str(output) + ".txt").read_text().splitlines()]
            actual = [(*r[:3], -1 if r[3] == (1 << 63) - 1 else r[3])
                      for r in actual]
        self.assertEqual(actual, expected)

    def simulate(self, path, policy, capacity=2, *options, ok=True):
        return self.run_tool(
            "cachesim", path, "oracleGeneral", policy, capacity,
            "--ignore-obj-size=1", "--hashpower=8", "--num-thread=1",
            *options, ok=ok,
        )

    def assert_ratio(self, result, n_req, misses):
        match = re.search(r"(\d+) req, miss ratio ([0-9.]+)", result.stdout)
        self.assertIsNotNone(match, result.stdout + result.stderr)
        self.assertEqual(int(match[1]), n_req)
        self.assertAlmostEqual(float(match[2]), misses / n_req, delta=0.000051)

    def test_reported_counterexample_and_retention_semantics(self):
        rows = [(0, 1, 100), (1, 1, 0)] + [
            (i + 2, 2 + i % 2, 100) for i in range(20)
        ]
        source = self.write_trace(rows)
        for policy in ("belady", "beladySize", "belady,lru"):
            result = self.simulate(source, policy, ok=False)
            self.assertIn("filtering zero-size records", result.stderr)
            self.assertIn("traceConv", result.stderr)
        for policy in ("fifo", "lru"):
            self.assert_ratio(self.simulate(source, policy), 21, 3)
        retained = self.simulate(source, "belady", 2,
                                 "-t", "ignore-size-zero-req=false")
        self.assert_ratio(retained, 22, 3)
        filtered = [r for r in rows if r[2] != 0]
        repaired = self.convert(source)
        self.assert_oracle(repaired, filtered)
        for policy in ("belady", "beladySize", "fifo", "lru"):
            self.assert_ratio(self.simulate(repaired, policy), 21, 3)

    def test_recompute_after_filter_and_before_size_normalization(self):
        # Leading/trailing zeros, zero chains, reaccess after a zero, and
        # pointers crossing unrelated zeros all need correct rebasing.
        rows = [(i, oid, size) for i, (oid, size) in enumerate([
            (8, 0), (1, 9), (1, 0), (2, 4), (1, 0), (1, 7),
            (3, 2), (2, 4), (9, 0), (1, 7), (7, 0),
        ])]
        source = self.write_trace(rows)
        for fmt in ("oracleGeneral", "lcs_v1", "lcs_v3"):
            for ignore in (False, True):
                with self.subTest(fmt=fmt, ignore=ignore):
                    repaired = self.convert(source, fmt,
                                            "--ignore-obj-size=" + str(int(ignore)))
                    filtered = [(t, oid, 1 if ignore else size)
                                for t, oid, size in rows if size]
                    self.assert_oracle(repaired, filtered, fmt)

    def test_no_zero_round_trip(self):
        rows = [(i, oid, 10) for i, oid in enumerate([1, 2, 3, 1, 4, 2, 1])]
        source = self.write_trace(rows)
        self.assertEqual(self.convert(source).read_bytes(), source.read_bytes())

    def test_csv_conversion(self):
        rows = [(0, 1, 0), (1, 1, 9), (2, 2, 3), (3, 1, 0),
                (4, 1, 9), (5, 3, 0)]
        source = self.work / "input.csv"
        source.write_text("time,id,size\n" + "".join(
            f"{t},{oid},{size}\n" for t, oid, size in rows))
        output = self.work / "from-csv.oracleGeneral"
        self.run_tool("traceConv", source, "csv", "-o", output,
                      "--output-format=oracleGeneral", "--ignore-obj-size=1",
                      "-t", "time-col=1,obj-id-col=2,obj-size-col=3,"
                      "obj-id-is-num=true,has-header=true")
        self.assert_oracle(output, [(t, oid, 1) for t, oid, size in rows if size])

    def test_explicit_retention_conversion(self):
        rows = [(0, 1, 5), (1, 1, 0), (2, 2, 7), (3, 1, 5)]
        source = self.write_trace(rows)
        retained = self.convert(source, "oracleGeneral",
                                "-t", "ignore-size-zero-req=false")
        self.assert_oracle(retained, rows)
        retained_lcs = self.convert(source, "lcs_v1",
                                    "-t", "ignore-size-zero-req=false")
        result = self.run_tool("cachesim", retained_lcs, "lcs", "belady", 2,
                               "--ignore-obj-size=1", "--hashpower=8", ok=False)
        self.assertIn("filtering zero-size records", result.stderr)

    @unittest.skipUnless(WITH_ZSTD and shutil.which("zstd"),
                         "requires zstd support and CLI")
    def test_compressed_oracle_guard_and_conversion_diagnostic(self):
        source = self.write_trace([(0, 1, 1), (1, 1, 0), (2, 2, 1)])
        compressed = Path(str(source) + ".zst")
        subprocess.run(["zstd", "-q", str(source), "-o", str(compressed)],
                       check=True)
        self.assert_ratio(self.simulate(compressed, "lru"), 2, 2)
        for policy in ("belady", "beladySize", "belady,lru"):
            result = self.simulate(compressed, policy, ok=False)
            self.assertIn("filtering zero-size records", result.stderr)
        result = self.run_tool("traceConv", compressed, "oracleGeneral", ok=False)
        self.assertIn("decompress", result.stderr)

    def test_trailing_and_long_zero_runs(self):
        rows = [(0, 1, 1)] + [(i + 1, 1, 0) for i in range(100000)]
        source = self.write_trace(rows)
        result = self.simulate(source, "lru")
        self.assert_ratio(result, 1, 1)
        self.assertEqual(result.stderr.count("skipping zero-size records"), 1)
        # Even when no retained request follows the skipped record, reject it.
        result = self.simulate(source, "belady", ok=False)
        self.assertIn("filtering zero-size records", result.stderr)
        repaired = self.convert(source)
        self.assert_oracle(repaired, rows[:1])

    def test_all_zero_rejected_by_conversion(self):
        source = self.write_trace([(0, 1, 0), (1, 2, 0)])
        for fmt in ("oracleGeneral", "lcs_v1"):
            result = self.run_tool("traceConv", source, "oracleGeneral",
                                   "--output-format=" + fmt, ok=False)
            self.assertIn("no requests remain after filtering", result.stderr)

    def test_minisim_rejects_stale_oracle(self):
        source = self.write_trace([(0, 1, 1), (1, 1, 0), (2, 2, 1)])
        for policy in ("belady", "beladySize"):
            result = self.run_tool(
                "mrcProfiler", source, "oracleGeneral", "--algo=" + policy,
                "--profiler=MINISIM", "--profiler-params=FIX_RATE,1,1",
                "--size=1B,2B,2", ok=False,
            )
            self.assertIn("filtering zero-size records", result.stderr)

    def test_small_traces_against_exhaustive_optimum(self):
        rng = random.Random(361)
        for trial in range(8):
            rows = [(i, rng.randrange(5), rng.choice([0, 1, 1, 1]))
                    for i in range(16)]
            rows.append((16, 1, 1))
            source = self.write_trace(rows)
            filtered = [r for r in rows if r[2]]
            repaired = self.convert(source)
            self.assert_oracle(repaired, filtered)
            ids = tuple(r[1] for r in filtered)
            for capacity in (1, 2, 3):
                with self.subTest(trial=trial, capacity=capacity):
                    self.assert_ratio(self.simulate(repaired, "belady", capacity),
                                      len(ids), optimal_misses(ids, capacity))


if __name__ == "__main__":
    resource.setrlimit(resource.RLIMIT_CORE, (0, 0))
    unittest.main()
