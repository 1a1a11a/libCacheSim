# Project Guidelines

## Code Style
- Treat `libCacheSim/` and `test/` as the primary source of truth for style. Most of the codebase is C; use C++17 only in areas that already use C++ wrappers or implementations.
- Follow the existing formatting and lint configuration in `.clang-format` and `.clang-tidy`. The project uses a Google-based format with 2-space indentation and an 80-column limit.
- Keep changes warning-free. The CMake build enables strict warning sets and `-Werror` for both C and C++.
- Prefer small, performance-aware changes in hot paths such as cache algorithms, trace readers, and profilers.

## Architecture
- `libCacheSim/cache/` contains cache implementations, eviction/admission/prefetch algorithms, and plugin support.
- `libCacheSim/traceReader/` contains trace parsers and reader dispatch; `libCacheSim/traceAnalyzer/`, `libCacheSim/profiler/`, and `libCacheSim/mrcProfiler/` contain analysis and miss-ratio tooling.
- `libCacheSim/dataStructure/` and `libCacheSim/utils/` provide shared infrastructure; the public C API starts at `libCacheSim/include/libCacheSim.h`.
- `libCacheSim/bin/` contains CLI entry points, `test/` contains CTest-backed unit tests, `example/` contains integration patterns, and `libCacheSim-node/` is a separate Node.js binding layer.

## Build And Test
- Prefer the workspace debug build for code changes: run the VS Code `build-debug` task or `bash scripts/debug.sh -c`. This configures and builds `_build_dbg/` with Ninja and the same strict warning flags used by the project.
- Use a standard out-of-source CMake build for release-style work: `cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release` then `cmake --build build`.
- CMake tests are enabled by default. Run `ctest --test-dir _build_dbg --output-on-failure` after the debug build, or the equivalent `build/` test directory if you used a separate build tree.
- If you change installation, packaging, or the public library surface, also review `test/test_lib.sh`.
- Use sample traces in `data/` for quick validation unless the task specifically requires the large traces in `2024_google/`.

## Project-Specific Conventions
- When adding a new eviction algorithm, reader, or plugin, follow `doc/advanced_lib_extend.md` instead of inventing a new integration path. These changes usually require updates to implementation files, registration headers, CMake lists, CLI/parser wiring, and tests.
- For API-level work, confirm the public interface against `doc/API.md` and existing examples in `example/cacheSimulator/`, `example/cacheHierarchy/`, and `example/cacheCluster/`.
- Optional features such as GLCache, LRB, and 3LCache have extra external dependencies. Do not enable or modify those paths unless the task requires them.
- Zstd trace support is on by default and is required for compressed traces. Release builds may link tcmalloc; the debug workflow avoids that to stay debugger-friendly.

## Docs To Link
- Installation and dependencies: `doc/install.md`
- CLI usage: `doc/quickstart_cachesim.md`
- Debug workflow: `doc/debug.md`
- Public API and library usage: `doc/API.md` and `doc/advanced_lib.md`
- Extension workflows: `doc/advanced_lib_extend.md` and `doc/quickstart_plugin.md`
- Trace and MRC tooling: `doc/quickstart_traceAnalyzer.md`, `doc/quickstart_traceUtils.md`, `doc/quickstart_mrcProfiler.md`, and `doc/performance.md`
