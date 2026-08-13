# libCacheSim Adoption Census

A source-linked inventory of where libCacheSim is used. Every entry quotes the sentence in
a primary source that supports it and links to that source. Entries without a checkable
source do not appear.

| | |
|---|---|
| **Census version** | `1.0.0` |
| **Census date** | 2026-08-13 |
| **Repository snapshot** | `develop` @ [`7c169cf`](https://github.com/1a1a11a/libCacheSim/commit/7c169cf596fa9aa7ad3a47dee18bc6b243db1834); latest release [`v0.3.5`](https://github.com/1a1a11a/libCacheSim/releases/tag/v0.3.5) (2026-03-15) |
| **Canonical location** | [`doc/adoption.md`](https://github.com/1a1a11a/libCacheSim/blob/develop/doc/adoption.md) |

**Scope.** A work is listed if a primary source shows it builds on, bundles, forks,
distributes, or runs libCacheSim. Quotations were matched against raw source text — not a
summary — and every GitHub link is pinned to a commit so a later upstream edit cannot
strand a quotation.

**Not adoption.** SIEVE and S3-FIFO were designed and evaluated with libCacheSim and are
now reimplemented in many third-party systems. Those systems are not libCacheSim users.
They, and work that borrows only the trace format or an algorithm implementation, are
recorded in [§6](#6-related-but-not-adoption) and counted separately.

---

## 1. First-party papers and artifacts

| Work | Venue | Evidence | Source |
|---|---|---|---|
| A large-scale analysis of hundreds of in-memory key-value cache clusters at Twitter — Yang, Yue, Rashmi | OSDI '20 / ACM TOS '21 | The paper that introduced the simulator: "We built an open-source simulator called libCacheSim [71] to study the steady-state miss ratio of the different eviction algorithms." Recorded as the origin, not as adoption. | [USENIX](https://www.usenix.org/conference/osdi20/presentation/yang), [ACM DL](https://doi.org/10.1145/3468521) |
| GL-Cache: Group-level learning for efficient and high-performance caching | FAST '23 | Artifact README, on the micro-implementation half: "It is a snapshot of [libCacheSim](https://github.com/1a1a11a/libCacheSim)." That directory reproduces libCacheSim's own README and `doc/` tree. | [Thesys-lab/fast23-GLCache](https://github.com/Thesys-lab/fast23-GLCache/tree/fbb824091209f390f7c921aadee1fbf0d35209c2), [USENIX](https://www.usenix.org/conference/fast23/presentation/yang-juncheng) |
| FIFO queues are all you need for cache eviction (S3-FIFO) | SOSP '23 | Artifact README: "The repo is a snapshot of [libCacheSim](https://github.com/cacheMon/libCacheSim), modified cachelib, and distComp." Simulations run through its `cachesim` binary. | [Thesys-lab/sosp23-s3fifo](https://github.com/Thesys-lab/sosp23-s3fifo/tree/6bc49d9630572721b41cd08adfa982775f3cb1de), [ACM DL](https://dl.acm.org/doi/10.1145/3600006.3613147) |
| SIEVE is Simpler than LRU | NSDI '24 | Artifact README lists its simulator as "a snapshot of libCacheSim", and directs readers here for build instructions. | [Thesys-lab/NSDI24-SIEVE](https://github.com/Thesys-lab/NSDI24-SIEVE/tree/7137cfef66a19dc7e65ba7575807616c1d942383), [USENIX](https://www.usenix.org/conference/nsdi24/presentation/zhang-yazhuo) |
| FIFO can be Better than LRU: the Power of Lazy Promotion and Quick Demotion (QD-LP) | HotOS '23 | Artifact README: "The repo is a snapshot of [libCacheSim](https://github.com/1a1a11a/libCacheSim), which contains the implementation of the algorithms compared in the paper." Ships here as [`QDLP.c`](/libCacheSim/cache/eviction/QDLP.c). | [Thesys-lab/HotOS23-QD-LP](https://github.com/Thesys-lab/HotOS23-QD-LP/tree/83aa8def9912a7b8d63fbfcda36bf2a648cc5cd5), [ACM DL](https://doi.org/10.1145/3593856.3595887) |
| Learning-Augmented Heuristics: Simple Yet Smart, Robust and Interpretable Cache Eviction (S4-FIFO) — Xia, Nixon, Marthen, Bhandari, Yang (Harvard, UIUC, U Chicago, ITB, Meta) | OSDI '26 | "We implement S4-FIFO in both libCacheSim for simulation-based evaluation and Meta Cachelib for prototype evaluation and production deployment"; "All miss ratio results are from libCacheSim because most state-of-the-art eviction algorithms are not available in open-source caches such as Cachelib." | [USENIX](https://www.usenix.org/conference/osdi26/presentation/xia), [cacheMon/osdi26-s4-fifo](https://github.com/cacheMon/osdi26-s4-fifo/tree/000095fc3c96a95ff1b9a19d101fb1411c7f4d54) |
| Demystifying and Improving Lazy Promotion in Cache Eviction — Chen, Qiu, Chen, Vinayak (CMU), Al-Araby (ITS Surabaya), Yang (Harvard) | PVLDB 19(4), 549–562 | "We implemented each Lazy Promotion technique on top of libCacheSim [5] and replayed the traces in our dataset to measure miss ratio." | [PVLDB](https://www.vldb.org/pvldb/vol19/p549-yang.pdf), [DOI](https://doi.org/10.14778/3785297.3785299), [cacheMon/Lazy-Promotions](https://github.com/cacheMon/Lazy-Promotions/tree/7e503984e41994f03880a12153b86827aaf7e680) |
| Clock2Q+: A Simple and Efficient Replacement Algorithm for Metadata Cache in VMware vSAN — Zhai, Marthen, Balivada, Bojji, Knauft, Rohilla, Zuo, Liu, Austruy, Wang, Yang | arXiv, Nov 2025 | "We implemented Clock2Q+ in the cache simulator libCacheSim [52]. This ensures a fair and accurate comparison with state-of-the-art algorithms." Ships here as [`Clock2QPlus.c`](/libCacheSim/cache/eviction/Clock2QPlus.c). | [arXiv:2511.21958](https://arxiv.org/abs/2511.21958) |
| Designing Efficient and Scalable Key-value Cache Management Systems — Juncheng Yang (PhD thesis, CMU-CS-24-149) | CMU, 2024 | "Simulator. We implemented S3-FIFO and the state-of-the-art eviction algorithms … in libCacheSim [368]." | [PDL](https://www.pdl.cmu.edu/ftp/Storage/CMU-CS-24-149-juncheny.pdf), [DOI](https://doi.org/10.1184/R1/28500515.v1) |

---

## 2. Third-party research

| Work | Venue | Evidence | Source |
|---|---|---|---|
| Writeback Modeling: Theory and Application to Zipfian Workloads — Smith, Ding (Rochester), Byrne (Michigan Tech) | MEMSYS '21 | "The simulator is based on libCacheSim [36] written in C++ compiled with g++ 5.4 (-O3)." The earliest third-party use in this edition. | [PDF](https://www.memsys.io/wp-content/uploads/2023/09/p2.pdf), [DOI](https://doi.org/10.1145/3488423.3519331) |
| FLOWS: Balanced MRC Profiling for Heterogeneous Object-Size Cache — Guo, Wang, Zhou (HUST), Jiang (UT Arlington), Han, Xing (Tencent) | EuroSys '24 | "To simulate the cache environment, we employ libCachesim [3] … We have extended libCachesim to incorporate additional features such as cache size adjustment, MRC generation, and cache instance balancing." | [author copy](https://ranger.uta.edu/~jiang/publication/Conferences/2024/Eurosys24\(FLOWS\).pdf), [DOI](https://doi.org/10.1145/3627703.3650078) |
| ScaleOPT: A Scalable Optimal Page Replacement Policy Simulator — Han, Lee, Son (Chung-Ang University) | POMACS 8(3) / SIGMETRICS '25 | Benchmarked against it: "ScaleOPT improves the simulation time by up to 6.3×, 7.7×, 20.5×, and 13.9× compared with … two widely-used cache simulators (webcachesim and libCacheSim)." From the publisher-deposited abstract; full text paywalled. | [DOI](https://doi.org/10.1145/3700426), [Crossref](https://api.crossref.org/works/10.1145/3700426) |
| Linear Elastic Caching via Ski Rental — Kumar, Lipcon, Purohit, Sarlos (Google) | CIDR '25 | "We implemented our ski rental based algorithms in libCacheSim [1]." The resulting policy — not libCacheSim itself — is reported as deployed in Spanner. | [CIDR](https://mail.vldb.org/cidrdb/papers/2025/p22-kumar.pdf) |
| 3L-Cache: Low Overhead and Precise Learning-based Eviction Policy for Caches — Zhou, Niu, Xiong, Fang, Wang (BJUT; Microsoft Research) | FAST '25 | "3L Cache is implemented in the [libCacheSim] library"; the repository is "Forked from LibCacheSim, which is a platform for cache evaluation". The algorithm was later upstreamed here. | [optiq-lab/3L-Cache](https://github.com/optiq-lab/3L-Cache/tree/134cd159b635cdab75419a4281bed1a330fef31f), [USENIX](https://www.usenix.org/conference/fast25/presentation/zhou-wenbin), [issue #119](https://github.com/1a1a11a/libCacheSim/issues/119), [`3LCache/`](/libCacheSim/cache/eviction/3LCache/) |
| Merlin: An Efficient Adaptive Cache Eviction Algorithm via Fine-Grained Characterization — Li, Guo, Fan, Wu, Zhang, Wang, Luo, Zhou (Peking University), Wang (Michigan Tech), Tamir (UCLA) | OSDI '26 | "We implemented Merlin in both CacheLib [11] … and libCacheSim [2]"; hit rates are evaluated in libCacheSim and flash-friendliness "on an extension of libCacheSim". The authors state they "contributed … an implementation of CAR to libCacheSim" — their claim; [`CAR.c`](/libCacheSim/cache/eviction/CAR.c) ships here, but this repository's history does not attribute it to them. | [USENIX](https://www.usenix.org/conference/osdi26/presentation/li-liujia) |
| Man-Made Heuristics Are Dead. Long Live Code Generators! (PolicySmith) — Dwivedula, Saxena, Akella, Chaudhuri, Kim (UT Austin) | HotNets '25 | "Our prototype is built on libCacheSim, a high-performance web cache simulator with an event-driven interface." Its artifact wires in a fork the same organisation maintains: `[submodule "webcache/libCacheSim"] … url = git@github.com:ldos-project/libcachesim.git`. | [ACM DL](https://doi.org/10.1145/3772356.3772413), [arXiv:2510.08803](https://arxiv.org/abs/2510.08803), [ldos-project/policysmith](https://github.com/ldos-project/policysmith/tree/6a0ee6dbd1cb388885b6be62995d54ef82bdd60e) |
| Vulcan: Instance-specialized, Verifiable Systems Heuristics Through LLM-driven Search — Dwivedula, Saxena, Yadalam, Campbell, Kim, Akella | arXiv, Dec 2025 | "The scaffolding, implemented on top of `libCacheSim`, is responsible for instantiating the queues that are a part of the topology." | [arXiv:2512.25065](https://arxiv.org/abs/2512.25065) |
| MetaMuse: Algorithm Generation via Creative Ideation — Ma, Liang, Gao, Yan (Microsoft Research) | ICLR '26 | "For cache replacement, these n traces are generated by libCacheSim (Yang et al. 2020), from different Zipfian distributions." Used as the trace generator. | [ICLR](https://proceedings.iclr.cc/paper_files/paper/2026/hash/85632be2cd69e9a0ef4ba054c096fac9-Abstract-Conference.html), [arXiv:2510.03851](https://arxiv.org/abs/2510.03851) |
| Intent-Driven Storage Systems: From Low-Level Tuning to High-Level Understanding — Bergman, Song, Cavigelli, Berestizshevsky, Zhou, Zhang | arXiv, Oct 2025 | "We then evaluated all the traces for all policies using libcachesim (Yang et al. 2020), configured with a cache size of 0.1% of the working set." | [arXiv:2510.15917](https://arxiv.org/abs/2510.15917) |
| DynamicAdaptiveClimb: Adaptive Cache Replacement with Dynamic Resizing — Berend, Dolev, Kumari, Mishra, Kogan-Sadetsky, Somani | arXiv, Nov 2025 | "Simulator: We conduct all evaluations using libCacheSim [46], an open-source, high-performance, and extensible cache simulator widely adopted in recent caching research." The authors' repository vendors the simulator and adds `AdaptiveClimb.c` and `DynamicAdaptiveClimb.c` under its eviction directory. | [arXiv:2511.21235](https://arxiv.org/abs/2511.21235), [Dhruv27Mishra/Adaptive-Climb](https://github.com/Dhruv27Mishra/Adaptive-Climb/tree/becd5d148840b2910d4d983d9ed775500ecb857d) |
| SCION: Size-aware Policy Orchestration for Nonstationary Object Caches — Qizhi Wang (PingCAP) | arXiv, 2026 | "We implement a trace-driven benchmark in C++ on top of libCacheSim [25]." Its prototype fetches upstream at a pinned commit and applies `patches/libcachesim-scion.patch`. | [arXiv:2605.01055](https://arxiv.org/abs/2605.01055), [Icemap/SCION](https://github.com/Icemap/SCION/tree/3f13ac3e397d686708cae00d227e5caa2d226548) |
| T3-LRU: Three-Tier Hotness-Aware Concurrent Cache Eviction Algorithm — Zhang Xin et al. | Research Square preprint, 2026 | Evaluated in a fork archived as software: "rim99/libCacheSim: T3LRU Simulation … a high performance library for building cache simulators & T3-LRU simulation added". | [preprint](https://doi.org/10.21203/rs.3.rs-10049449/v1), [Zenodo](https://doi.org/10.5281/zenodo.20713182), [rim99/libCacheSim](https://github.com/rim99/libCacheSim/tree/b15718edf79b8fbf2e86256266f57e64289164f8) |

---

## 3. Practitioner and community use

Use outside the publication record.

| Who | What | Evidence | Source |
|---|---|---|---|
| Ben Manes — maintainer of [Caffeine](https://github.com/ben-manes/caffeine) | Ran libCacheSim beside Caffeine's own simulator to re-check published S3-FIFO/SIEVE results, and reported a size-accounting discrepancy back to this project | "I used libcachesim at 0f4d135 (current master) … with this patch to include the new trace formats." | [issue #18](https://github.com/1a1a11a/libCacheSim/issues/18) |
| Marc Brooker — engineer at Amazon Web Services | Designed a SIEVE-k variant, evaluated it here, and published his implementation as a fork | "Using the excellent open source libCacheSim I tried SIEVE-2 against SIEVE on a range of real-world traces"; "I've implemented SIEVE-k in a fork of libCacheSim." The fork carries [`Sieve_k.c`](https://github.com/mbrooker/libCacheSim/blob/0147d1e93d411165492554a4e2cccbcb1d610fb0/libCacheSim/cache/eviction/Sieve_k.c). | [brooker.co.za](https://brooker.co.za/blog/2023/12/15/sieve.html), [mbrooker/libCacheSim](https://github.com/mbrooker/libCacheSim/tree/0147d1e93d411165492554a4e2cccbcb1d610fb0) |
| L. Stampf — BSc thesis, Vrije Universiteit Amsterdam, 2025 | Compared eviction primitives on Zipf-like workloads, and fixed the macOS build upstream while doing it | "The building and execution of cache simulations was performed using the cache simulator provided by libCacheSim." Its upstream PRs [#179](https://github.com/1a1a11a/libCacheSim/pull/179) and [#181](https://github.com/1a1a11a/libCacheSim/pull/181) were merged; [#183](https://github.com/1a1a11a/libCacheSim/pull/183) was not. | [thesis PDF](https://www.cs.vu.nl/~wanf/theses/stampf-bscthesis.pdf) |
| Bintang Dwi Marthen (ITB) | Chameleon Cloud artifact implementing CLOCK-Pro | "This artifact is my implementation of ClockPro on libCacheSim" | [Trovi](https://chameleoncloud.org/experiment/share/1a05c09b-f149-4555-b133-a4114155746b) |
| Raden Rafly Hanggaraksa Budiarto (ITB) | Chameleon Cloud artifact implementing CAR | "Implemented the CAR (Clock with Adaptive Replacement) cache implementation on LibCacheSim" | [Trovi](https://chameleoncloud.org/experiment/share/bac62a10-3868-4a77-9075-7e9247dd199b) |
| Muhammad Haekal Muhyidin Al-Araby (ITS) | Chameleon Cloud artifact demonstrating AdaptSize | "This artifact contain an example on how to run AdaptSize on LibCacheSim." | [Trovi](https://chameleoncloud.org/experiment/share/f9cdc812-0c61-46ab-848d-295e47a4954c) |
| `gws8820` | Fork extending the simulator to two cache levels | README: "runs in 2-Level so can set different replacement algorithm and cache size for each level." | [gws8820/2-Level-libCacheSim](https://github.com/gws8820/2-Level-libCacheSim/tree/d93106ca12b6b7f9593ab1db7aaa5482c73356df) |

The three Chameleon artifacts were found by scanning all 460 public Trovi artifacts and
are dated within ten days of each other. Marthen and Al-Araby are co-authors on the
OSDI '26 and PVLDB papers above, so they are project-affiliated; Budiarto has no
established link and counts as third-party, which is the default when none can be shown.

---

## 4. Ecosystem projects and datasets

| Project | Evidence | Source |
|---|---|---|
| **CacheBench** — benchmarking suite evaluating 18 eviction algorithms across thousands of traces, built by Haocheng Xia under the UCSC OSPO Summer of Reproducibility | Describes libCacheSim as a core component, and the project as "a Python package that allows users to easily download traces and run simulation analyses using libCacheSim" | [UCSC OSPO report](https://ucsc-ospo.github.io/report/osre25/harvard/cachebench/2025-08-06-haochengxia/) |
| **system-intelligence-benchmark** — suite scoring LLM-designed systems heuristics; its `cache_algo_bench` task scores candidate eviction policies | Shells out to the built binary: `{LIBCACHSIM_PATH}/_build/bin/cachesim {cache_trace} oracleGeneral {cache_alg} {cache_cap} …` | [sys-intelligence/system-intelligence-benchmark](https://github.com/sys-intelligence/system-intelligence-benchmark/tree/46596edd8113a3eaf5646e49a42cc2a9fae3de4d) |
| **cache_dataset** — production cache traces (Meta, Twitter, CloudPhysics, Microsoft, Wikimedia, Alibaba, Tencent) | Ships tutorial notebooks that run libCacheSim, and publishes traces in its `oracleGeneral` format | [cacheMon/cache_dataset](https://github.com/cacheMon/cache_dataset/tree/a005343f26f47110de5c8d78d645ee89bee1e7ed) |

CacheBench and cache_dataset are project-affiliated; system-intelligence-benchmark is not.

---

## 5. Distribution

The project publishing itself — reach rather than third-party adoption.

| Channel | Package | State on 2026-08-13 | Source |
|---|---|---|---|
| PyPI | `libcachesim` (Python bindings, cacheMon org) | Latest `0.3.3.post4` (2026-02-17); first release 2025-07-14; 5 releases; Python ≥ 3.10 | [PyPI](https://pypi.org/project/libcachesim/), [cacheMon/libCacheSim-python](https://github.com/cacheMon/libCacheSim-python/tree/7079d279873dff1052ad716670e7a49841f9d1e4) |
| npm | `libcachesim-node` (in-tree at [`libCacheSim-node/`](/libCacheSim-node)) | Latest `0.3.2` (2025-07-14); first publish 2025-06-18 | [npm](https://www.npmjs.com/package/libcachesim-node) |
| Docker Hub | `1a1a11a/libcachesim`, from the repository's [`dockerfile`](/dockerfile) | 1,014 pulls; the only libCacheSim image on Docker Hub | [Docker Hub](https://hub.docker.com/r/1a1a11a/libcachesim) |
| GitHub Releases | source releases | 5 releases, `v0.1` → `v0.3.5` (2026-03-15) | [Releases](https://github.com/1a1a11a/libCacheSim/releases) |

**Downloads.** npm: 173 over
[2025-08-10 → 2026-08-09](https://api.npmjs.org/downloads/range/2025-08-10:2026-08-09/libcachesim-node),
a fixed window that reproduces exactly. PyPI: 1,805 over 2026-02-12 → 2026-08-11 with
`mirrors=false`, a **point-in-time observation** — [pypistats](https://pypistats.org/packages/libcachesim)
serves only a rolling ~180-day window, so that interval ages out and the number cannot be
re-derived later. The two are filtered differently and neither excludes CI traffic; treat
both as weak proxies, not user counts.

**No third-party redistribution exists.** AUR, conda-forge, vcpkg, conan-center-index,
Debian, Homebrew, Nix, and crates.io carry no libCacheSim port; the single Docker Hub
image is the project's own; deps.dev and GitHub's dependency graph report zero reverse
dependencies for the PyPI package.

---

## 6. Related but not adoption

**Borrowed implementations and trace formats.** These take something from libCacheSim
without running or building on it, and count toward no total above — recorded because
`oracleGeneral` is becoming a de-facto interchange format for cache traces.

| Project | What it takes | Evidence | Source |
|---|---|---|---|
| **Cache is King: Smart Page Eviction with eBPF** — Zussman et al. (Columbia; IBM Research) | The LHD implementation, ported to eBPF | "We implement LHD using cachebpf, based on the implementation in libcachesim [69, 70, 72]." The paper shows no run of the tool. | [arXiv:2502.02750](https://arxiv.org/abs/2502.02750) |
| **Pelikan** `cachesim` (Rust) | The binary trace formats | "cachesim can import libCacheSim's binary trace formats"; "The `op` column uses the same integer encoding as libCacheSim's `req_op_e`" | [pelikan-io/cachesim](https://github.com/pelikan-io/cachesim/tree/9997462b2b3746fbd826837702027afbdea57d7a) |
| **Otter** (Go) | The trace formats | A `libcachesim` parser package: `OracleGeneralFormat = "oracleGeneral"`, `LibcachesimCSVFormat = "libcachesimCSV"` | [maypok86/otter](https://github.com/maypok86/otter/tree/8c526307556486ea0337280a4211135720bc29cc) |
| **Caffeine** (Java) | The trace formats | Registers `LCS_TRACE`, `LCS_ORACLE_GENERAL`, `LCS_TWITTER` readers, backed by a `parser/libcachesim/` package tree | [ben-manes/caffeine](https://github.com/ben-manes/caffeine/tree/9da6581ee366aa63c51e0dc96692d02f9c29ccff) |

**Downstream algorithm adoption.** SIEVE and S3-FIFO are reimplemented in third-party
systems. **Those systems do not use libCacheSim** — conflating the two is the most likely
way this census gets misread.

| System | Evidence | Source |
|---|---|---|
| Ceph | `src/common/web_cache.h`: "The implementation is based on SIEVE [0] with additional TTL", citing the NSDI '24 paper | [ceph/ceph](https://github.com/ceph/ceph/blob/5995d21863b3992bd9f463b5a0f774869351be36/src/common/web_cache.h) |
| TiDB | `pkg/infoschema/sieve.go` implements a SIEVE cache | [pingcap/tidb](https://github.com/pingcap/tidb/blob/d5f9ca5690c0a53cac36002f9d2d2bdcba25f4fc/pkg/infoschema/sieve.go) |

A broader list — immudb, DragonFly, dnscrypt-proxy, PostgREST, Pelikan, SkiftOS, Nyrkiö,
and 20+ language-level cache libraries — is maintained on the
[SIEVE project site](https://cachemon.github.io/SIEVE-website/); it is project-maintained
and partly self-reported, and apart from the two rows above was not independently verified.

---

## 7. Repository signals

From the GitHub API on 2026-08-13. These measure attention, not deployment.

| Signal | Value |
|---|---|
| Stars | 339 |
| Forks | 111 |
| Contributors | 36 |
| Open issues (excluding pull requests) | 21 |
| Open pull requests | 8 |
| Created | 2020-06-19 |
| License | GPL-3.0 |

The issue and PR counts are split because the API's `open_issues_count` field — 29 here —
sums both, and reading it as an issue count overstates the backlog. Forks are not counted
as adoption: their READMEs were fetched and almost all are dormant copies of upstream
text. The ones carrying real work are named in [§2](#2-third-party-research) and
[§3](#3-practitioner-and-community-use) instead.

---

## How to cite this census

This file is revised in place, so the `develop` URL always resolves to the newest edition.
To cite the edition you read, pin it to a commit: open the file on GitHub and press
<kbd>y</kbd>, or run `git log -1 --format=%H -- doc/adoption.md` in a clone. Each edition's
permalink is the commit that bumped its version in the changelog below.

```bibtex
@techreport{libcachesim-adoption-census-2026,
  title       = {libCacheSim Adoption Census},
  author      = {{libCacheSim maintainers}},
  institution = {libCacheSim project},
  number      = {census v1.0.0},
  year        = {2026},
  month       = aug,
  % replace <commit> with the permalink of the edition you read
  url         = {https://github.com/1a1a11a/libCacheSim/blob/<commit>/doc/adoption.md},
  note        = {Census date 2026-08-13}
}
```

`7c169cf` in the header is the repository state the census describes, not a permalink for
this file — it predates the file and will not resolve. To cite libCacheSim itself, use
[`references.md`](/references.md).

**To add an entry:** open a PR with the source URL, the verbatim sentence located in raw
source, and the date you verified it. Bump the version and append to the changelog.

---

## Changelog

| Version | Date | Change |
|---|---|---|
| 1.0.0 | 2026-08-13 | First edition. 31 entries — 8 first-party papers and artifacts, 13 third-party works, 7 practitioner and community entries, 3 ecosystem projects — of which 19 are third-party, plus 4 distribution channels. Separately recorded and not counted: the OSDI '20 paper that introduced the simulator, 4 borrowed-implementation and trace-format rows, 2 downstream algorithm adopters, and repository signals. |
