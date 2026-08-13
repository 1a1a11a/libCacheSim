# Contributing to libCacheSim

Thanks for your interest in libCacheSim! Bug reports, new algorithms, trace readers, documentation fixes, and performance work are all welcome.

## Before you start

* **Small fixes** — typos, broken links, an obvious bug — just open a pull request.
* **Larger changes** — a new algorithm, an API change, a new subsystem — please [open an issue](https://github.com/1a1a11a/libCacheSim/issues) first so we can agree on the approach before you invest the time.
* **Questions** — use [Discussions](https://github.com/1a1a11a/libCacheSim/discussions) rather than the issue tracker.

## Development setup

Install the dependencies ([glib](https://developer.gnome.org/glib/), [tcmalloc](https://github.com/google/tcmalloc), [zstd](https://github.com/facebook/zstd)) and build out-of-source:

```bash
bash scripts/install_dependency.sh          # see doc/install.md if this does not work
cmake -G Ninja -B _build -DCMAKE_BUILD_TYPE=Release
ninja -C _build
```

Binaries land in `_build/bin/`. The commands in the docs are written to be run from `_build/`, so the sample traces in [`data/`](data/) are at `../data/`:

```bash
cd _build && ./bin/cachesim ../data/cloudPhysicsIO.vscsi vscsi lru 1gb
```

For day-to-day work on the C/C++ code, prefer the debug build — it uses the same strict warning flags as CI and skips tcmalloc so it stays debugger-friendly:

```bash
bash scripts/debug.sh -c        # configure + build _build_dbg/
bash scripts/debug.sh -- data/cloudPhysicsIO.vscsi vscsi lru,s3fifo 100mb,1gb
```

See [doc/debug.md](doc/debug.md) for the full debugging workflow.

### Install the pre-commit hook

```bash
bash scripts/setup_hooks.sh
```

The hook runs clang-format, clang-tidy, and a strict-warning compile on the files you staged, which is roughly what CI will check. Use `SKIP_LINT=1 git commit ...` to bypass it in a pinch. Logs are kept in `.lint-logs/`.

## Testing

Tests are CTest-backed and run by default:

```bash
ctest --test-dir _build --output-on-failure --parallel 4
```

CI additionally builds Ubuntu with LeakSanitizer, so please check that new allocations are freed. If you touch installation, packaging, or the public library surface, also run [`test/test_lib.sh`](test/test_lib.sh).

Every new eviction, admission, or prefetching algorithm needs a test in the matching file under [`test/`](test/) — for eviction algorithms that is [`test/test_evictionAlgo.c`](test/test_evictionAlgo.c).

Changes to the command-line tools need their own coverage, and the library tests will not catch them: option parsing, `-e print` parameter reporting, and eviction parameter validation all live between the command line and the point where the C tests build a cache. The `testCLI` target covers that ground — if you add an algorithm parameter or a CLI option, add a case there. Check that invalid input is rejected with a message rather than a signal, since `ERROR()` aborts and a deliberate rejection is easy to confuse with a crash. It runs from the build directory and skips itself if the binaries or the sample traces are not where it expects, so it can also be run by hand:

```bash
cd _build && bash ../test/test_cli.sh
```

## Code style

* The project follows **Google style**: 2-space indent, 80-column limit, configured in [`.clang-format`](.clang-format) and [`.clang-tidy`](.clang-tidy). Run `clang-format -i <file>` before committing.
* Most of the codebase is **C**. Use C++17 only where the surrounding code already does (`cache/eviction/cpp/`, `LHD/`, `LRB/`, the analyzer and profiler binaries).
* **Keep the build warning-free.** CI compiles with `-Wall -Wextra -Werror` plus an extended warning set.
* Cache algorithms, trace readers, and profilers are hot paths — prefer changes that do not add per-request work.

## Adding something new

Follow the existing integration path rather than inventing one; these changes usually touch the implementation, a registration header, the CMake lists, the CLI wiring, and a test.

| What | Guide |
| --- | --- |
| Eviction / admission / prefetch algorithm | [doc/advanced_lib_extend.md](doc/advanced_lib_extend.md) |
| Trace reader | [doc/advanced_lib_extend.md](doc/advanced_lib_extend.md) |
| Algorithm without recompiling (Python/C plugin) | [doc/quickstart_plugin.md](doc/quickstart_plugin.md) |
| Public API change | [doc/API.md](doc/API.md), [doc/advanced_lib.md](doc/advanced_lib.md) |

When you add an algorithm, also list it in the [README](README.md#supported-algorithms) with the name users pass on the command line.

## Pull requests

1. Branch off `develop` — that is the default branch and where PRs are merged.
2. Keep the PR focused; unrelated cleanups are easier to review separately.
3. Make sure `ctest` passes and the build is warning-free.
4. Describe *what* changed and *why*. If it affects miss ratios or throughput, include the numbers and the command you ran.
5. CI must be green before merge.

## Reporting bugs

Please use the [bug report template](.github/ISSUE_TEMPLATE/bug_report.md) and include the trace format, the exact command line, and the build type. A reproducer against one of the sample traces in `data/` is ideal, since those ship with the repo.

Security issues should **not** be filed as public issues — see [SECURITY.md](SECURITY.md).

## License

libCacheSim is [Apache-2.0](LICENSE) licensed. By contributing, you agree that your contributions are licensed under the same terms.
