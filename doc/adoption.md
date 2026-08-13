# libCacheSim Adoption Census

A source-linked inventory of where libCacheSim is used. Every entry carries a link to
the primary source that supports it, plus the class of evidence that link provides.
Entries without a checkable source do not appear.

| | |
|---|---|
| **Census version** | `1.0.0` |
| **Census date** | 2026-08-13 |
| **Repository snapshot** | `develop` @ [`7c169cf`](https://github.com/1a1a11a/libCacheSim/commit/7c169cf596fa9aa7ad3a47dee18bc6b243db1834); latest release [`v0.3.5`](https://github.com/1a1a11a/libCacheSim/releases/tag/v0.3.5) (2026-03-15) |
| **Verification window** | All sources in this edition were fetched and re-read on 2026-08-13 |
| **Canonical location** | [`doc/adoption.md`](https://github.com/1a1a11a/libCacheSim/blob/develop/doc/adoption.md) in this repository |

This file is the versioned artifact: it is revised in place, each revision bumps the
census version and appends to the [changelog](#changelog), and any revision can be
cited by its commit permalink. See [How to cite this census](#how-to-cite-this-census).

---

## 1. Scope and inclusion criteria

**In scope — adoption of libCacheSim itself.** A project, paper, or package is listed
only if a primary source shows it *builds on, bundles, forks, distributes, or runs*
libCacheSim.

**Out of scope — adoption of algorithms that happen to have been developed here.**
SIEVE and S3-FIFO were designed and evaluated with libCacheSim, and both are now
reimplemented in many third-party systems. Those systems are *not* libCacheSim users;
they are downstream of the research. They are tracked separately and explicitly
labelled in [§7](#7-downstream-algorithm-adoption-not-libcachesim-adoption).

**Evidence classes.** Every row states which class its source supports:

| Class | Meaning |
|---|---|
| **A — vendored / forked** | The source tree contains a copy or fork of libCacheSim |
| **B — built on** | The work states it is implemented on top of, or extends, libCacheSim |
| **C — used as-is** | The work states it ran libCacheSim as its simulator or harness |
| **D — distributes** | The project ships libCacheSim to users as a package or binding |
| **E — cites only** | The work cites libCacheSim without a checkable statement of use |

Class E entries are recorded but are *not* counted as adoption; they are listed under
[§8](#8-examined-and-not-included) so that a later edition does not re-litigate them.

---

## 2. Research artifacts and publications

### 2.1 First-party (authored by libCacheSim maintainers)

| Work | Venue | Class | Evidence | Source |
|---|---|---|---|---|
| GL-Cache: Group-level learning for efficient and high-performance caching | FAST '23 | A | Artifact README on the micro-implementation: "It is a snapshot of [libCacheSim](https://github.com/1a1a11a/libCacheSim)." The directory reproduces libCacheSim's own README and `doc/` tree. | [Thesys-lab/fast23-GLCache](https://github.com/Thesys-lab/fast23-GLCache), [USENIX](https://www.usenix.org/conference/fast23/presentation/yang-juncheng) |
| FIFO queues are all you need for cache eviction (S3-FIFO) | SOSP '23 | A | Artifact README: "The repo is a snapshot of [libCacheSim](https://github.com/cacheMon/libCacheSim), modified cachelib, and distComp." Simulations are run through its `cachesim` binary. | [Thesys-lab/sosp23-s3fifo](https://github.com/Thesys-lab/sosp23-s3fifo), [ACM DL](https://dl.acm.org/doi/10.1145/3600006.3613147) |
| SIEVE is Simpler than LRU | NSDI '24 | A | Artifact README lists its simulator as "a snapshot of libCacheSim", and directs readers to libCacheSim for build instructions. | [Thesys-lab/NSDI24-SIEVE](https://github.com/Thesys-lab/NSDI24-SIEVE), [USENIX](https://www.usenix.org/conference/nsdi24/presentation/zhang-yazhuo) |

The remaining first-party papers in [`references.md`](/references.md) (OSDI '20 workload
analysis, HotOS '23 QD-LP) ship their algorithms in this repository — e.g.
[`QDLP.c`](/libCacheSim/cache/eviction/QDLP.c) — but their standalone artifacts were not
re-verified for this edition, so they are not tabulated above.

### 2.2 Third-party

| Work | Venue / date | Class | Evidence | Source |
|---|---|---|---|---|
| 3L-Cache: Low Overhead and Precise Learning-based Eviction Policy for Caches — Wenbin Zhou, Zhixiong Niu, Yongqiang Xiong, Juan Fang, Qian Wang (Beijing Univ. of Technology; Microsoft Research) | FAST '25 | A + B | Artifact README: "3L-Cache is implemented in the libCacheSim library, and its experimental environment configuration is consistent with libCacheSim"; repository layout notes "Forked from LibCacheSim". The algorithm was subsequently upstreamed into this repository. | [optiq-lab/3L-Cache](https://github.com/optiq-lab/3L-Cache), [USENIX](https://www.usenix.org/conference/fast25/presentation/zhou-wenbin), [issue #119](https://github.com/1a1a11a/libCacheSim/issues/119), [`3LCache/`](/libCacheSim/cache/eviction/3LCache/) |
| Man-Made Heuristics Are Dead. Long Live Code Generators! (PolicySmith) — Dwivedula, Saxena, Akella, Chaudhuri, Kim | arXiv, Oct 2025 | B | "Our prototype is built on libCacheSim, a high-performance web cache simulator with an event-driven interface." | [arXiv:2510.08803](https://arxiv.org/abs/2510.08803) |
| Vulcan: Instance-specialized, Verifiable Systems Heuristics Through LLM-driven Search — Dwivedula, Saxena, Yadalam, Campbell, Kim, Akella | arXiv, Dec 2025 | B | "The scaffolding, implemented on top of `libCacheSim`, is responsible for instantiating the queues that are a part of the topology"; libCacheSim measures object hit rate for candidate heuristics. | [arXiv:2512.25065](https://arxiv.org/abs/2512.25065) |
| DynamicAdaptiveClimb: Adaptive Cache Replacement with Dynamic Resizing — Berend, Dolev, Kumari, Mishra, Kogan-Sadetsky, Somani | arXiv, Nov 2025 | C | "We use the libcachesim simulator for our experiments." | [arXiv:2511.21235](https://arxiv.org/abs/2511.21235) |
| SCION: Size-aware Policy Orchestration for Nonstationary Object Caches — Qizhi Wang (PingCAP) | arXiv, 2026 | B | "We implement a trace-driven benchmark in C++ on top of libCacheSim"; the authors also "integrated AdaptiveClimb/DynamicAdaptiveClimb into libCacheSim". | [arXiv:2605.01055](https://arxiv.org/abs/2605.01055) |

### 2.3 Independent cross-validation

| Who | What | Class | Evidence | Source |
|---|---|---|---|---|
| Ben Manes (maintainer, [Caffeine](https://github.com/ben-manes/caffeine)) | Ran libCacheSim side-by-side with Caffeine's simulator to re-check published S3-FIFO/SIEVE hit-ratio results, and reported a size-accounting discrepancy back to this project | C | "I used libcachesim at 0f4d135 (current master) … with this patch to include the new trace formats." | [issue #18](https://github.com/1a1a11a/libCacheSim/issues/18) |

This is the clearest instance of libCacheSim being used *adversarially* by a third party
— an outside maintainer reproducing the project's own numbers — which makes it worth
recording separately from citation-style use.

---

## 3. Distribution channels

| Channel | Package | Class | State on 2026-08-13 | Source |
|---|---|---|---|---|
| PyPI | `libcachesim` (Python bindings, maintained under the cacheMon org) | D | Latest `0.3.3.post4` (2026-02-17); first release `0.3.2` (2025-07-14); 5 releases; requires Python ≥ 3.10; GPLv3+ | [PyPI](https://pypi.org/project/libcachesim/), [cacheMon/libCacheSim-python](https://github.com/cacheMon/libCacheSim-python) |
| npm | `libcachesim-node` (Node.js bindings, in-tree at [`libCacheSim-node/`](/libCacheSim-node)) | D | Latest `0.3.2` (2025-07-14); first publish `1.0.0` (2025-06-18); MIT | [npm](https://www.npmjs.com/package/libcachesim-node) |
| GitHub Releases | source releases | D | 5 releases, `v0.1` → `v0.3.5` (2026-03-15) | [Releases](https://github.com/1a1a11a/libCacheSim/releases) |

**Download volume** (PyPI, mirrors excluded, via [pypistats.org](https://pypistats.org/packages/libcachesim)):
1,805 downloads over the 2026-02-12 → 2026-08-11 window available from the API, 258 of
them in the trailing 30 days. npm `libcachesim-node`: 173 downloads in the trailing
year ([npm downloads API](https://api.npmjs.org/downloads/point/last-year/libcachesim-node)).
Download counts include CI and mirror traffic and are a weak proxy for use; they are
recorded for trend-tracking across census editions, not as a user count.

---

## 4. Ecosystem projects and datasets

| Project | Class | Evidence | Source |
|---|---|---|---|
| **CacheBench** — benchmarking suite evaluating 18 eviction algorithms across thousands of traces, developed by Haocheng Xia (UIUC, visiting Harvard) under the UCSC OSPO Summer of Reproducibility | B | Report describes libCacheSim as a core component and the project as "a Python package that allows users to easily download traces and run simulation analyses using libCacheSim" | [UCSC OSPO report](https://ucsc-ospo.github.io/report/osre25/harvard/cachebench/2025-08-06-haochengxia/) |
| **cache_dataset** — open collection of production cache traces (Meta, Twitter, CloudPhysics, Microsoft, Wikimedia, Alibaba, Tencent) | C | Ships three tutorial notebooks that run libCacheSim — "Using libCacheSim to read the dataset", "…to analyze and plot the trace", "…to run cache simulation". The dataset itself is *format-compatible* rather than built on the library: "We provide both plain text format … and `oracleGeneral` format that is suitable for using with [libCacheSim] platform." | [cacheMon/cache_dataset](https://github.com/cacheMon/cache_dataset) |

Note the class distinction between these two rows: CacheBench is implemented on top of libCacheSim (B), whereas cache_dataset publishes traces in a libCacheSim-readable format and drives the tool from tutorials (C). Shipping a compatible format alone would not qualify for either class.

---

## 5. Repository signals

Aggregate signals from the GitHub API on 2026-08-13. These measure attention and
contribution, not deployment, and are listed apart from sourced adoption for that reason.

| Signal | Value |
|---|---|
| Stars | 339 |
| Forks | 111 |
| Contributors (non-anonymous) | 36 |
| Open issues | 28 |
| Created | 2020-06-19 |
| License | GPL-3.0 |

Forks were enumerated and are *not* treated as adoption: nearly all are dormant
snapshots with no divergent description or activity. Known research forks that do carry
independent work are listed by name in [§2.2](#22-third-party) instead.

---

## 6. Known gaps and limitations

Stated so that later editions know what this one did not cover.

1. **No repository-wide code search.** This edition was assembled without a GitHub-wide
   code search for `libCacheSim` includes or CMake references. Every count here is
   therefore a **lower bound**, and the third-party table is biased toward work that
   names the library in prose.
2. **No citation-graph sweep.** Scholar/Semantic Scholar citation lists were not
   enumerated; papers were found by targeted search and then verified individually.
   Papers that use libCacheSim without saying so are invisible to this method.
3. **Private and internal use is unobservable.** Industry use behind closed doors leaves
   no source to link, so it is absent by construction.
4. **Paywalled sources are excluded, not judged.** See
   [§8](#8-examined-and-not-included).
5. **libCacheSim has no DOI.** No Zenodo (or equivalent) archival record was found for
   the software, and the repository carries no `CITATION.cff`. Citation of the software
   currently relies on the repository URL and the BibTeX in
   [`references.md`](/references.md); citation of *this census* relies on the commit
   permalink below. Minting a DOI would give both a stable archival identifier.

---

## 7. Downstream algorithm adoption (not libCacheSim adoption)

SIEVE (NSDI '24) and S3-FIFO (SOSP '23) were developed and evaluated with libCacheSim,
and are now reimplemented in third-party systems. **These systems do not use
libCacheSim.** They are recorded because they are the research output's reach, and
conflating the two is the most likely way this census could be misread.

Spot-verified for this edition:

| System | Evidence | Source |
|---|---|---|
| Ceph | `src/common/web_cache.h` header comment: "The implementation is based on SIEVE [0] with additional TTL", citing the NSDI '24 paper directly; contains `SieveQueue`, `_sieve_hand`, `sieve_evict()` | [ceph/ceph](https://github.com/ceph/ceph/blob/main/src/common/web_cache.h) |
| TiDB | `pkg/infoschema/sieve.go` implements a SIEVE cache (per-entry `visited` flag) | [pingcap/tidb](https://github.com/pingcap/tidb/blob/master/pkg/infoschema/sieve.go) |

A broader list — immudb, DragonFly, dnscrypt-proxy, PostgREST, Pelikan, SkiftOS,
Nyrkiö, plus 20+ language-level cache libraries — is maintained on the
[SIEVE project site](https://cachemon.github.io/SIEVE-website/). That list is
project-maintained and partly self-reported; apart from the two rows above, its entries
were **not** independently verified for this edition.

---

## 8. Examined and not included

Candidates that surfaced during the sweep and were rejected. Recording the rejections is
part of the census: it prevents re-checking and makes the inclusion bar auditable.

| Candidate | Reason |
|---|---|
| RAC: Relation-Aware Cache Replacement for Large Language Models ([arXiv:2602.21547](https://arxiv.org/abs/2602.21547)) | Full text contains no reference to libCacheSim |
| CacheMind ([arXiv:2602.12422](https://arxiv.org/abs/2602.12422)) | Uses ChampSim and gem5; no libCacheSim reference |
| LearnedCache ([arXiv:2605.26168](https://arxiv.org/abs/2605.26168)) | eBPF/`cache_ext`-based; no libCacheSim reference |
| [project-kona/KCacheSim](https://github.com/project-kona/KCacheSim) | Name collision only — built on Valgrind's Cachegrind, unrelated to this project |
| PyPI [`cachebench`](https://pypi.org/project/cachebench/) | Name collision only — an unrelated LLM prompt-cache observability package, not the CacheBench in [§4](#4-ecosystem-projects-and-datasets) |
| SL-Cache (Springer, [10.1007/978-981-92-0363-5_36](https://doi.org/10.1007/978-981-92-0363-5_36)) | Plausible but **unverified** — full text is paywalled, so no usage statement could be read. Recheck in a later edition |

---

## 9. Reproducing this census

Repository signals:

```bash
curl -s https://api.github.com/repos/1a1a11a/libCacheSim |
  python3 -c "import sys,json;d=json.load(sys.stdin);print({k:d[k] for k in ['stargazers_count','forks_count','open_issues_count','created_at']})"

# contributor count: read the rel="last" page number
curl -sI "https://api.github.com/repos/1a1a11a/libCacheSim/contributors?per_page=1" | grep -i '^link'
```

Package state:

```bash
curl -s https://pypi.org/pypi/libcachesim/json      | python3 -m json.tool | head -40
curl -s https://registry.npmjs.org/libcachesim-node | python3 -m json.tool | head -40
curl -s "https://pypistats.org/api/packages/libcachesim/overall?mirrors=false"
```

Publication evidence: each row's source URL was opened and the quoted sentence read in
place. A row is only promoted past class E when such a sentence exists.

**To add an entry:** open a PR editing this file with (a) the source URL, (b) the
verbatim sentence supporting the claim, (c) the evidence class, and (d) the date you
verified it. Bump the census version and append to the changelog below.

---

## How to cite this census

This document is revised in place, so the `develop` URL below always resolves to the
*newest* edition. To cite the exact edition you read, resolve it to a commit permalink
first — a document cannot contain its own commit hash, so the pin is something the
citing reader produces:

- **On GitHub:** open the file and press <kbd>y</kbd>. The URL rewrites from
  `.../blob/develop/doc/adoption.md` to `.../blob/<commit>/doc/adoption.md`, which is
  immutable.
- **From a clone:** `git log -1 --format=%H -- doc/adoption.md` gives the commit of the
  edition you have checked out.
- **Rule:** each edition's permalink is the commit that bumped its census version — the
  same commit that added its row to the [changelog](#changelog). Earlier editions stay
  reachable through the file's History view.

Fill the pinned commit into the `url` field when citing:

```
libCacheSim Adoption Census, version 1.0.0 (census date 2026-08-13).
libCacheSim project documentation, doc/adoption.md.
Newest edition: https://github.com/1a1a11a/libCacheSim/blob/develop/doc/adoption.md
Cited edition:  https://github.com/1a1a11a/libCacheSim/blob/<commit>/doc/adoption.md
```

```bibtex
@techreport{libcachesim-adoption-census-2026,
  title       = {libCacheSim Adoption Census},
  author      = {{libCacheSim maintainers}},
  institution = {libCacheSim project},
  type        = {Project documentation},
  number      = {census v1.0.0},
  year        = {2026},
  month       = aug,
  % replace <commit> with the permalink commit of the edition you read (see above);
  % https://github.com/1a1a11a/libCacheSim/blob/develop/doc/adoption.md is the newest
  url         = {https://github.com/1a1a11a/libCacheSim/blob/<commit>/doc/adoption.md},
  note        = {Census date 2026-08-13; repository snapshot at census time 7c169cf}
}
```

`7c169cf` in the header is the repository state the census describes, **not** a
permalink for this file — that commit predates the file and will not resolve.

To cite libCacheSim itself, use the BibTeX entries in [`references.md`](/references.md).

---

## Changelog

| Version | Date | Change |
|---|---|---|
| 1.0.0 | 2026-08-13 | First edition. **14 sourced entries** — 9 research (3 first-party artifacts, 5 third-party works, 1 independent cross-validation), 3 distribution channels, 2 ecosystem projects — of which 8 are third-party. By evidence class: 4×A, 5×B, 3×C, 3×D (15 class assignments over 14 rows — the 3L-Cache row carries both A and B). 6 candidates examined and rejected. Repository signals and downstream algorithm adoption recorded separately and excluded from these counts. |
