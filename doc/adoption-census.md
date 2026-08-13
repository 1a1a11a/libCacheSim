# libCacheSim Adoption Census

**Census version:** 1.0.2
**Snapshot date:** 2026-08-13
**Maintained at:** `doc/adoption-census.md` in [1a1a11a/libCacheSim](https://github.com/1a1a11a/libCacheSim)

A source-linked inventory of documented libCacheSim adoption. Every entry below carries a
public URL, the quoted evidence that supports it, the date the evidence was checked, and a
confidence grade. Entries that could not be verified are recorded as such rather than dropped
silently, and claims that were checked and found to be false are listed in
[Checked and excluded](#6-checked-and-excluded).

This document is versioned with the source tree, so any commit of this file is a citable
snapshot. See [How to cite this census](#how-to-cite-this-census).

---

## 1. Methodology

### What counts as adoption

An entry qualifies as **direct adoption** only if a public artifact shows libCacheSim itself
being built on, forked, linked, or executed — not merely cited in a bibliography. Papers that
list libCacheSim as related work without using it are excluded.

Adoption of *algorithms designed and evaluated in libCacheSim* (S3-FIFO, SIEVE, QD-LP) is
tracked separately in [Section 5](#5-algorithm-lineage-not-libcachesim-adoption). A system that
implements SIEVE is **not** a libCacheSim user. Conflating the two would inflate this census by
an order of magnitude, and the distinction is the single most important integrity rule here.

### Confidence grades

| Grade | Meaning |
| --- | --- |
| **A** | Primary source, quoted verbatim, states use of libCacheSim unambiguously. |
| **B** | Primary source confirms the relationship, but the extent of use is inferred. |
| **C** | Self-reported or aggregate claim with no public per-entry roster to audit. |
| **U** | Lead identified but not verifiable by automated fetch; retained for manual follow-up. |

### Verification protocol

Each source URL was fetched and read on the verification date shown. Where a source is a PDF,
the full text was searched for `libCacheSim` / `libcachesim` before the entry was accepted. Two
candidate papers surfaced by search engines were rejected this way (Section 6).

Third-party lists are not taken on trust: every per-file link in
[Section 5.2](#52-named-sieve-adopters-with-direct-source-links) was fetched individually rather
than inherited from the upstream list, which is how the two ungraded entries there were found.

### Known limitations

1. **No GitHub-wide code search was performed.** This session's tooling was scoped to the
   libCacheSim repository, so source-level forks and vendored copies across GitHub are
   undercounted. A future revision run with repository-search access should add a
   `code search: "libCacheSim" in build files` pass — this is the largest known gap.
2. **Package dependency graphs miss this ecosystem.** libCacheSim is typically consumed by
   building from source or forking, not by declaring a dependency. GitHub's dependency graph for
   the Python binding reported **0 repositories and 0 packages** on 2026-08-12
   ([source](https://github.com/cacheMon/libCacheSim-python/network/dependents)) — a measured
   negative, not an absence of users. Treat dependent counts as a floor of zero information.
3. **Private and industrial use is invisible.** Commercial users appear here only if they publish.
   The aggregate figures in [Section 4](#4-self-reported-aggregate-claims) exist precisely because
   that population is not publicly enumerable.
4. **Download statistics are absent.** pypistats.org returned HTTP 429 during this snapshot; PyPI
   download counts should be added in the next revision.
5. **External evidence links are branch URLs, not commit permalinks.** The file-level links in
   [Section 5.2](#52-named-sieve-adopters-with-direct-source-links) point at `main`/`master`, so
   they can drift away from what was verified — the same failure this document warns about for its
   own citation. The SkiftOS row is that failure already realized: its linked path returned 404 on
   re-check. Pinning was attempted for this snapshot and could not be completed —
   `api.github.com` was unreachable (HTTP 403) from the environment that ran it, and GitHub blob
   pages render the commit SHA client-side, so no SHA could be read. The next revision should pin
   each link with `git ls-remote` or
   `GET /repos/{owner}/{repo}/commits?path={file}&per_page=1`, rewriting
   `blob/main/...` to `blob/{sha}/...`. Until then, treat Section 5.2's Grade A entries as verified
   **as of 2026-08-13**, not as permanently reproducible.

---

## 2. Distribution and repository signals

Measured facts about how libCacheSim is published and consumed. All figures fetched 2026-08-12.

| Signal | Value | Source | Grade |
| --- | --- | --- | --- |
| GitHub stars | 339 | [github.com/1a1a11a/libCacheSim](https://github.com/1a1a11a/libCacheSim) | A |
| GitHub forks | 111 | [github.com/1a1a11a/libCacheSim](https://github.com/1a1a11a/libCacheSim) | A |
| PyPI package | `libcachesim`, latest `0.3.3.post4` released 2026-02-17; 5 released versions; requires Python >=3.10; GPLv3+ | [pypi.org/project/libcachesim](https://pypi.org/project/libcachesim/) · [JSON API](https://pypi.org/pypi/libcachesim/json) | A |
| PyPI maintainers | `1a1a11a`, `hxia7` | [pypi.org/project/libcachesim](https://pypi.org/project/libcachesim/) | A |
| npm package | `libcachesim-node`, latest `0.3.2`; first published 2025-06-18, latest 2025-07-14; MIT | [registry.npmjs.org/libcachesim-node](https://registry.npmjs.org/libcachesim-node) | A |
| Python binding repo | [cacheMon/libCacheSim-python](https://github.com/cacheMon/libCacheSim-python), docs at [cachemon.github.io/libCacheSim-python](https://cachemon.github.io/libCacheSim-python/) | [pypi.org/project/libcachesim](https://pypi.org/project/libcachesim/) | A |
| Dependency-graph dependents (Python binding) | 0 repositories, 0 packages | [dependents graph](https://github.com/cacheMon/libCacheSim-python/network/dependents) | A |

Star and fork counts are point-in-time and will drift; re-measure rather than citing these
numbers as current.

---

## 3. Confirmed direct users

Public artifacts that build on, fork, or run libCacheSim. Verified 2026-08-12.

### 3.1 PolicySmith — LLM-generated cache policies (UT Austin)

- **Artifact:** *Man-Made Heuristics Are Dead. Long Live Code Generators!*,
  [arXiv:2510.08803](https://arxiv.org/abs/2510.08803) (2025-10-13). Dwivedula, Saxena, Akella,
  Chaudhuri, Kim.
- **Evidence (quoted):** "Our prototype is built on libCacheSim, a high-performance web cache
  simulator with an event-driven interface." ([full text](https://arxiv.org/html/2510.08803v1))
- **Use:** libCacheSim is the execution substrate for evaluating LLM-generated eviction
  heuristics against established baselines.
- **Code:** [github.com/ldos-project/policysmith](https://github.com/ldos-project/policysmith)
- **Grade:** A · **Independent of upstream:** yes

### 3.2 3L-Cache — FAST '25 (learning-based eviction)

- **Artifact:** *3L-Cache: Low Overhead and Precise Learning-based Eviction Policy for Caches*,
  USENIX FAST '25 ([program page](https://www.usenix.org/conference/fast25/presentation/zhou-wenbin)).
  Artifact repository: [github.com/optiq-lab/3L-Cache](https://github.com/optiq-lab/3L-Cache).
- **Evidence (quoted):** "3L-Cache is implemented in the libCacheSim library, and its experimental
  environment configuration is consistent with libCacheSim." The repository layout is annotated
  "3L-Cache/ -- Forked from LibCacheSim, which is a platform for cache evaluation."
- **Use:** Whole-project fork of libCacheSim used as the evaluation platform (reported 4855 traces,
  twelve comparison policies).
- **Upstreamed:** tracked in [issue #119](https://github.com/1a1a11a/libCacheSim/issues/119)
  (opened 2025-01-23, closed 2025-02-20); the algorithm now ships in-tree at
  [`libCacheSim/cache/eviction/3LCache/`](../libCacheSim/cache/eviction/3LCache/).
- **Grade:** A · **Independent of upstream:** yes (fork authored externally; integration by upstream)

### 3.3 DynamicAdaptiveClimb (Ben-Gurion University et al.)

- **Artifact:** *DynamicAdaptiveClimb: Adaptive Cache Replacement with Dynamic Resizing*,
  [arXiv:2511.21235v1](https://arxiv.org/abs/2511.21235) (2025-11-26). Berend, Dolev, Kumari,
  Mishra, Kogan-Sadetsky, Somani.
- **Evidence (quoted):** "We conduct all evaluations using libCacheSim, an open-source,
  high-performance, and extensible cache simulator widely adopted in recent caching research."
  ([full text](https://arxiv.org/html/2511.21235v1))
- **Use:** All reported evaluation — 1067 traces across six datasets (Alibaba, TencentCBS, Wiki,
  Twitter, MetaCDN, Meta KV), miss ratio and multi-threaded throughput, against thirteen policies.
- **Grade:** A · **Independent of upstream:** yes

### 3.4 SCION (PingCAP)

- **Artifact:** *SCION: Size-aware Policy Orchestration for Nonstationary Object Caches*,
  [arXiv:2605.01055v1](https://arxiv.org/abs/2605.01055). Qizhi Wang, PingCAP
  Data & AI-Innovation Lab. Submission history on the arXiv abstract page reads
  "Fri, 27 Mar 2026"; note that this does not match the `2605` (May 2026) identifier prefix.
  The date above is arXiv's own stated submission date, not an inference from the identifier.
- **Evidence (quoted):** "We implement a trace-driven benchmark in C++ on top of libCacheSim."
  The authors additionally report integrating AdaptiveClimb/DynamicAdaptiveClimb into libCacheSim
  and building a trace conversion pipeline.
- **Use:** Benchmark harness over 30 public traces from `cache_dataset`, >5M requests each.
- **Grade:** A · **Independent of upstream:** yes (industrial affiliation, not an upstream maintainer)

### 3.5 CacheBench (UCSC OSPO reproducibility program)

- **Artifact:** *Midterm Report: Simulation, Comparison, and Conclusion of Cache Eviction*,
  UCSC OSPO project report, 2025-08-06, Haocheng Xia (UIUC, visiting Harvard).
  [Report](https://ucsc-ospo.github.io/report/osre25/harvard/cachebench/2025-08-06-haochengxia/)
- **Evidence (quoted):** "At the core of CacheBench lie two key components: the high-performance
  cache simulator, libCacheSim" — users "run simulation analyses using libCacheSim and the cache
  datasets."
- **Use:** libCacheSim is one of the two core components of a benchmarking suite built over the
  open cache trace datasets.
- **Grade:** A · **Independent of upstream:** **no** — the author is also an upstream contributor
  (closed [issue #119](https://github.com/1a1a11a/libCacheSim/issues/119)) and a PyPI maintainer of
  the Python binding. Counted as affiliated use, not third-party validation.

### Summary

| Adopter | Type | Relationship | Grade | Independent |
| --- | --- | --- | --- | --- |
| PolicySmith (UT Austin) | Research prototype | Built on | A | Yes |
| 3L-Cache (FAST '25) | Research artifact | Fork, upstreamed | A | Yes |
| DynamicAdaptiveClimb | Research evaluation | All evaluation | A | Yes |
| SCION (PingCAP) | Industrial research | Benchmark on top of | A | Yes |
| CacheBench | Benchmark suite | Core component | A | No (affiliated) |

Four of five confirmed direct users are independent of the upstream project.

---

## 4. Self-reported aggregate claims

These figures come from author-supplied biographies. They are consistent across two independent
hosts and across time, but no public roster backs them, so they cannot be audited entry by entry.
They are recorded as **Grade C** and should be attributed, not asserted as fact.

| Claim (quoted) | Source | Date checked | Grade |
| --- | --- | --- | --- |
| "the open-source cache simulation library he created, libCacheSim, has been used by almost 100 research institutes and companies." | [junchengyang.com](https://junchengyang.com/) | 2026-08-12 | C |
| "the open-source cache simulation library libCacheSim he created has been used by close to 100 research institutes and companies." | [Georgia Tech SCS seminar announcement, 2024-04-16](https://ic.gatech.edu/events/2024/04/16/scs-faculty-candidate-seminar-juncheng-yang) | 2026-08-12 | C |

The two statements agree in substance and differ only in wording ("almost" vs. "close to"),
across a roughly two-year span. Both trace to the same author, so they are one claim with two
publication venues, not two independent measurements.

---

## 5. Algorithm lineage (not libCacheSim adoption)

S3-FIFO and SIEVE were designed and evaluated in libCacheSim, and libCacheSim ships reference
implementations ([`S3FIFO.c`](../libCacheSim/cache/eviction/S3FIFO.c),
[`Sieve.c`](../libCacheSim/cache/eviction/Sieve.c)). Systems below adopted the **algorithms**.
They are recorded here because the lineage is real and traceable, and excluded from Section 3
because implementing an algorithm is not using the simulator.

### 5.1 Aggregate statements

| Claim (quoted) | Source | Date checked | Grade |
| --- | --- | --- | --- |
| "S3-FIFO and SIEVE are adopted for production at Google, VMware, Redpanda, and several others, with over 60 open-source libraries and packages in 18 programming languages available on GitHub." | [junchengyang.com](https://junchengyang.com/) | 2026-08-12 | C |
| "These algorithms have seen broad industry adoption — including in Android, the TiDB database, and many others — and have been implemented in dozens of open-source systems and libraries, including over 60 across more than 16 programming languages on GitHub." | [Harvard SEAS news, 2025-10-27](https://seas.harvard.edu/news/2025/10/juncheng-yang-winner-acm-award-dissertation-most-impact) | 2026-08-12 | C |

Both are aggregate claims with no public per-entry roster, which is Grade C by this document's
rubric. A university news office is editorially independent of the researcher, but that affects
who vouches for the claim, not whether it can be audited entry by entry — so it does not lift the
grade. Note that one system named in the second claim, TiDB, is independently verified at Grade A
in Section 5.2; the aggregate figures around it are not.

### 5.2 Named SIEVE adopters with direct source links

Entries originate from the SIEVE project's adopters list
([sievecache.com](https://sievecache.com/), which redirects to
[cachemon.github.io/SIEVE-website](https://cachemon.github.io/SIEVE-website/)), fetched 2026-08-12.
**Every link below was then fetched individually on 2026-08-13** and graded on what that fetch
actually showed — the list itself is treated as a lead, not as evidence.

| System | Evidence link | What the fetch showed | Grade |
| --- | --- | --- | --- |
| immudb | [PR #1971](https://github.com/codenotary/immudb/pull/1971) | "Replace LRU with SIEVE replacement policy", merged 2024-05-17 | A |
| TiDB | [`pkg/infoschema/sieve.go`](https://github.com/pingcap/tidb/blob/master/pkg/infoschema/sieve.go) | `type Sieve[K comparable, V any] struct`; comment cites the SIEVE paper | A |
| Nyrkiö | [`backend/core/sieve.py`](https://github.com/nyrkio/nyrkio/blob/main/backend/core/sieve.py) | "An implementation of the SIEVE cache eviction algorithm" | A |
| Dragonfly | [`src/core/compact_object.h`](https://github.com/dragonflydb/dragonfly/blob/main/src/core/compact_object.h#L124) | `TOUCHED` hot/cold bit, comment links `nsdi24-SIEVE.pdf` | A |
| dnscrypt-proxy | [`dnscrypt-proxy/plugin_cache.go`](https://github.com/DNSCrypt/dnscrypt-proxy/blob/master/dnscrypt-proxy/plugin_cache.go) | Imports `go-sieve-cache`; uses `sievecache.NewSharded` | A |
| encrypted-dns-server | [`src/cache.rs`](https://github.com/DNSCrypt/encrypted-dns-server/blob/master/src/cache.rs) | `use sieve_cache::SieveCache` | A |
| PostgREST | [JWT cache docs](https://docs.postgrest.org/en/latest/references/auth.html#jwt-cache) | "The JWT cache is bounded and uses the SIEVE algorithm for efficient eviction." | A |
| Ceph | [`src/common/web_cache.h`](https://github.com/ceph/ceph/blob/main/src/common/web_cache.h) | "The implementation is based on SIEVE [0] with additional TTL expiration support"; cites NSDI '24 | A |
| Pelikan | [pelikan-io/pelikan](https://github.com/pelikan-io/pelikan) | Upstream list links only the repository root; no specific implementing file located | **U** |
| SkiftOS | [`src/libs/karm-base/sieve.h`](https://github.com/skift-org/skift/blob/main/src/libs/karm-base/sieve.h) | Path returns **HTTP 404** — link rot since the entry was added | **U** |

Eight of ten resolve to a primary artifact naming SIEVE. The two graded **U** are retained as
leads: they may well be genuine adopters, but the links as published do not substantiate them.

The same source lists roughly twenty further standalone SIEVE cache libraries across Rust, Go,
Java, C#, Swift, Zig, D, Elixir, Nim, Ruby, Python, JavaScript/TypeScript and C++; see the
[full list](https://cachemon.github.io/SIEVE-website/) rather than duplicating it here. Those
were **not** individually fetched and carry no grade in this revision.

**Caveat:** the upstream adopters list is maintained by the SIEVE authors, and its completeness
is not independently audited. The 404 above shows the practical failure mode — entries are
accurate when added, then drift as the linked code moves.

---

## 6. Checked and excluded

Recorded so that future revisions do not re-investigate the same dead ends.

| Candidate | Why it surfaced | Finding | Disposition |
| --- | --- | --- | --- |
| *RAC: Relation-Aware Cache Replacement for LLMs*, [arXiv:2602.21547](https://arxiv.org/pdf/2602.21547) | Returned by a search for libCacheSim evaluations | Full-text search found no mention of libCacheSim | Excluded |
| *2DIO: A Cache-Accurate Storage Microbenchmark*, [arXiv:2603.19971](https://arxiv.org/pdf/2603.19971) | Returned by a search for libCacheSim usage | Full-text search found no mention of libCacheSim | Excluded |
| Chameleon Trovi artifact [`1a05c09b…`](https://trovi.chameleoncloud.org/dashboard/artifacts/1a05c09b-f149-4555-b133-a4114155746b) ("Clock-Pro Implementation on libCacheSim") | Title indicates libCacheSim use | Page is client-rendered; content could not be retrieved by fetch, and the API path returned 404 | **U** — manual check needed |
| Chameleon Trovi artifact [`bac62a10…`](https://trovi.chameleoncloud.org/dashboard/artifacts/bac62a10-3868-4a77-9075-7e9247dd199b) ("Clock with Adaptive Replacement Cache Implementation") | Title indicates libCacheSim use | Same as above | **U** — manual check needed |
| PyPI download statistics | Would quantify consumption | pypistats.org returned HTTP 429 | Deferred to next revision |
| USENIX-hosted PDFs (SIEVE NSDI '24, 3L-Cache FAST '25) | Primary sources for evaluation details | Fetches returned HTTP 403 from this environment | Substituted with artifact repositories and program pages |

---

## 7. How to update this census

1. Bump **Census version** (semver: patch for corrections, minor for new entries, major for a
   changed methodology) and set a new **Snapshot date**.
2. For every new entry, record: artifact identity, a verbatim quote showing libCacheSim use, the
   source URL, the verification date, a confidence grade, and whether the adopter is independent
   of the upstream project.
3. Re-fetch every source before restating it. Do not carry an unverified entry forward with a
   fresh date.
4. Move anything that fails re-verification into [Section 6](#6-checked-and-excluded) with the
   reason, rather than deleting it.
5. Keep Sections 3 and 5 strictly separate: simulator use versus algorithm use.
6. Add a row to the changelog below.

Priority work for the next revision, in order of expected yield: repository-wide code search for
forks and vendored copies (limitation 1), pinning Section 5.2's evidence links to commit SHAs
(limitation 5), PyPI/npm download statistics (limitation 4), manual verification of the two Trovi
artifacts, and full-text retrieval of the USENIX PDFs from an environment that can reach
usenix.org.

Note that limitations 4 and 5 were both blocked by the *environment* the snapshot ran in, not by
the sources themselves. Re-running from a host with unrestricted access to `api.github.com`,
`pypistats.org`, and `usenix.org` would close three open items in a single pass.

---

## How to cite this census

This file is versioned with the source tree; cite the census version together with the commit
that contains it. **Always cite a commit permalink, never a branch URL** — a branch moves, so a
`blob/develop` link does not identify the snapshot you actually read.

Get the SHA of the commit that last changed this file:

```bash
git log -1 --format=%H -- doc/adoption-census.md
```

Then substitute it for `<commit-sha>`:

```bibtex
@misc{libcachesim-adoption-census,
  title        = {libCacheSim Adoption Census},
  version      = {1.0.2},
  howpublished = {\url{https://github.com/1a1a11a/libCacheSim/blob/<commit-sha>/doc/adoption-census.md}},
  note         = {Snapshot dated 2026-08-13},
  year         = {2026}
}
```

Plain-text form:

> libCacheSim Adoption Census, version 1.0.2, snapshot 2026-08-13,
> `doc/adoption-census.md` in github.com/1a1a11a/libCacheSim at commit `<commit-sha>`.

For reading rather than citing, the current version always lives at
[`doc/adoption-census.md` on `develop`](https://github.com/1a1a11a/libCacheSim/blob/develop/doc/adoption-census.md).

---

## Changelog

| Version | Date | Change |
| --- | --- | --- |
| 1.0.2 | 2026-08-13 | Added the missing `Date checked` column to Section 5.1. Recorded a new limitation 5: Section 5.2's evidence links are branch URLs, not commit permalinks, so they can drift from what was verified — the SkiftOS 404 is that failure already realized. Pinning was attempted and blocked by the snapshot environment (`api.github.com` returned 403; blob pages render SHAs client-side), so the method is documented for the next revision instead of being left implicit. |
| 1.0.1 | 2026-08-13 | Individually fetched all ten Section 5.2 adopter links instead of inheriting them: 8 graded A against primary artifacts, Pelikan and SkiftOS downgraded to U (repo-root-only link; HTTP 404 link rot). Added grades to Section 5.2 so every entry carries one, as the introduction promises. Cited SCION by its arXiv abstract page and flagged that arXiv's stated submission date disagrees with its identifier prefix. Citation example now uses a commit permalink rather than a branch URL. Downgraded the Harvard SEAS aggregate claim from B to C: editorial independence does not make an aggregate claim auditable, and the rubric grades auditability. |
| 1.0.0 | 2026-08-12 | Initial census: 5 confirmed direct users, 6 distribution signals, 2 self-reported aggregate claims, 10 named algorithm-lineage adopters, 6 excluded or deferred candidates. |
