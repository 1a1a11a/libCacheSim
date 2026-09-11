# Security Policy

## Supported Versions

Security fixes are applied to the most recent release only. If you are running an older version, please upgrade before reporting.

## Reporting a Vulnerability

**Please do not report security issues in public issues, pull requests, or discussions.**

Report privately through GitHub: go to the [Security tab](https://github.com/1a1a11a/libCacheSim/security) of this repository and choose **Report a vulnerability**. This opens a private advisory visible only to the maintainers.

Please include:

* a description of the issue and its impact;
* the version or commit hash affected, and the build configuration (compiler, `CMAKE_BUILD_TYPE`, any optional features such as `ENABLE_GLCACHE` / `ENABLE_LRB` / `ENABLE_3L_CACHE`);
* steps to reproduce — ideally a trace file or a generator for one, plus the exact command line;
* any known mitigations or a suggested fix.

Because impact depends heavily on how libCacheSim is deployed, please indicate an urgency level of **Critical**, **High**, **Medium**, or **Low** and say why.

## What to expect

libCacheSim is maintained by a small group of researchers, so responses are best-effort rather than on a fixed schedule. We will acknowledge your report, tell you whether we consider it a vulnerability, and let you know when a fix lands. We are happy to credit you in the advisory unless you prefer otherwise.

## Scope

libCacheSim is a simulation and analysis library. It parses trace files, which are the main untrusted input: memory-safety bugs reachable from a malformed or malicious trace (in the trace readers, the CLI tools, or the cache implementations) are in scope.

Out of scope:

* crashes caused by deliberately invalid command-line arguments;
* resource exhaustion from legitimately large traces or cache sizes;
* issues in third-party code vendored under `libCacheSim/dataStructure/` or `libCacheSim/cache/eviction/{LHD,LRB,3LCache}/` — please report those upstream, though we appreciate a heads-up.

For non-security bugs, please use the [issue tracker](https://github.com/1a1a11a/libCacheSim/issues).
