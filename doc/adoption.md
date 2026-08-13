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
labelled in [§6](#6-downstream-algorithm-adoption-not-libcachesim-adoption).

**Evidence classes.** Every adoption row states which class its source supports. Rows in
the non-adoption categories described below carry no class.

| Class | Meaning |
|---|---|
| **A — vendored / forked** | The source tree contains a copy or fork of libCacheSim |
| **B — built on** | The work states it is implemented on top of, or extends, libCacheSim |
| **C — used as-is** | The work states it ran libCacheSim as its simulator or harness |
| **D — distributes** | The project ships libCacheSim to users as a package or binding |
| **E — cites only** | The work cites libCacheSim without a checkable statement of use |

Class E entries would be recorded but *not* counted as adoption. This edition has none —
no work was found that cites libCacheSim and does nothing else with it. Two other kinds of
reference do appear, and neither is class E nor an adoption class:

- the **origin row** in [§2.1](#21-first-party-authored-by-libcachesim-maintainers), the
  paper that introduced the simulator, which carries no class at all; and
- the **derived-implementation and interoperability rows**, which take libCacheSim's
  algorithm code or trace format without running or building on the software.

Both are marked in place and counted toward nothing.

---

## 2. Research artifacts and publications

### 2.1 First-party (authored by libCacheSim maintainers)

| Work | Venue | Class | Evidence | Source |
|---|---|---|---|---|
| A large-scale analysis of hundreds of in-memory key-value cache clusters at Twitter — Yang, Yue, Rashmi | OSDI '20 / ACM TOS '21 | — | The paper that introduced the simulator: "We built an open-source simulator called libCacheSim [71] to study the steady-state miss ratio of the different eviction algorithms." Recorded as the origin of the software, not as adoption of it. | [USENIX](https://www.usenix.org/conference/osdi20/presentation/yang), [ACM DL](https://doi.org/10.1145/3468521) |
| GL-Cache: Group-level learning for efficient and high-performance caching | FAST '23 | A | Artifact README, on the micro-implementation half of the artifact: "It is a snapshot of [libCacheSim](https://github.com/1a1a11a/libCacheSim)." That directory reproduces libCacheSim's own README and `doc/` tree. | [Thesys-lab/fast23-GLCache](https://github.com/Thesys-lab/fast23-GLCache/tree/fbb824091209f390f7c921aadee1fbf0d35209c2), [USENIX](https://www.usenix.org/conference/fast23/presentation/yang-juncheng) |
| FIFO queues are all you need for cache eviction (S3-FIFO) | SOSP '23 | A | Artifact README: "The repo is a snapshot of [libCacheSim](https://github.com/cacheMon/libCacheSim), modified cachelib, and distComp." Simulations are run through its `cachesim` binary. | [Thesys-lab/sosp23-s3fifo](https://github.com/Thesys-lab/sosp23-s3fifo/tree/6bc49d9630572721b41cd08adfa982775f3cb1de), [ACM DL](https://dl.acm.org/doi/10.1145/3600006.3613147) |
| SIEVE is Simpler than LRU | NSDI '24 | A | Artifact README lists its simulator as "a snapshot of libCacheSim", and directs readers to libCacheSim for build instructions. | [Thesys-lab/NSDI24-SIEVE](https://github.com/Thesys-lab/NSDI24-SIEVE/tree/7137cfef66a19dc7e65ba7575807616c1d942383), [USENIX](https://www.usenix.org/conference/nsdi24/presentation/zhang-yazhuo) |
| FIFO can be Better than LRU: the Power of Lazy Promotion and Quick Demotion (QD-LP) | HotOS '23 | A | Artifact README: "The repo is a snapshot of [libCacheSim](https://github.com/1a1a11a/libCacheSim), which contains the implementation of the algorithms compared in the paper." The algorithm also ships here as [`QDLP.c`](/libCacheSim/cache/eviction/QDLP.c). | [Thesys-lab/HotOS23-QD-LP](https://github.com/Thesys-lab/HotOS23-QD-LP/tree/83aa8def9912a7b8d63fbfcda36bf2a648cc5cd5), [ACM DL](https://doi.org/10.1145/3593856.3595887) |
| Learning-Augmented Heuristics: Simple Yet Smart, Robust and Interpretable Cache Eviction (S4-FIFO) — Haocheng Xia (Harvard; UIUC), William Nixon (U Chicago; Harvard), Bintang Dwi Marthen (Harvard; Institut Teknologi Bandung), Pranav Bhandari (Meta), Juncheng Yang (Harvard) | OSDI '26 | A + C | "We implement S4-FIFO in both libCacheSim for simulation-based evaluation and Meta Cachelib for prototype evaluation and production deployment." Miss ratios come from here: "All miss ratio results are from libCacheSim because most state-of-the-art eviction algorithms are not available in open-source caches such as Cachelib." The artifact README adds: "The repo is a snapshot of [libCacheSim](https://github.com/cacheMon/libCacheSim), modified cachelib, and distComp." | [USENIX](https://www.usenix.org/conference/osdi26/presentation/xia), [cacheMon/osdi26-s4-fifo](https://github.com/cacheMon/osdi26-s4-fifo/tree/000095fc3c96a95ff1b9a19d101fb1411c7f4d54) |
| Demystifying and Improving Lazy Promotion in Cache Eviction — Qinghan Chen, Ziyue Qiu, Zhuofan Chen, Rashmi Vinayak (CMU), Muhammad Haekal Muhyidin Al-Araby (ITS Surabaya), Juncheng Yang (Harvard) | PVLDB 19(4), 549–562 | B | "Miss Ratio Measurement. We implemented each Lazy Promotion technique on top of libCacheSim [5] and replayed the traces in our dataset to measure miss ratio." Artifact README: "`simulator/`: Contains the implementation of various lazy promotion techniques on a cache simulator based on [libCacheSim]." | [PVLDB](https://www.vldb.org/pvldb/vol19/p549-yang.pdf), [DOI](https://doi.org/10.14778/3785297.3785299), [cacheMon/Lazy-Promotions](https://github.com/cacheMon/Lazy-Promotions/tree/7e503984e41994f03880a12153b86827aaf7e680) |
| Clock2Q+: A Simple and Efficient Replacement Algorithm for Metadata Cache in VMware vSAN — Zhai, Marthen, Balivada, Bojji, Knauft, Rohilla, Zuo, Liu, Austruy, Wang, Yang (VMware and academic) | arXiv, Nov 2025 | C | "We implemented Clock2Q+ in the cache simulator libCacheSim [52]. This ensures a fair and accurate comparison with state-of-the-art algorithms." The algorithm now ships here as [`Clock2QPlus.c`](/libCacheSim/cache/eviction/Clock2QPlus.c). | [arXiv:2511.21958](https://arxiv.org/abs/2511.21958) |
| Designing Efficient and Scalable Key-value Cache Management Systems — Juncheng Yang (PhD thesis, CMU-CS-24-149) | CMU, 2024 | C | "We built an open-source simulator called libCacheSim [368] to study the steady-state miss ratio of the different eviction algorithms"; "Simulator. We implemented S3-FIFO and the state-of-the-art eviction algorithms … in libCacheSim [368]." | [PDL](https://www.pdl.cmu.edu/ftp/Storage/CMU-CS-24-149-juncheny.pdf), [DOI](https://doi.org/10.1184/R1/28500515.v1) |

The OSDI '20 workload analysis in [`references.md`](/references.md) has no separate
libCacheSim-based artifact recorded here.

### 2.2 Third-party

| Work | Venue / date | Class | Evidence | Source |
|---|---|---|---|---|
| 3L-Cache: Low Overhead and Precise Learning-based Eviction Policy for Caches — Wenbin Zhou, Zhixiong Niu, Yongqiang Xiong, Juan Fang, Qian Wang (Beijing Univ. of Technology; Microsoft Research) | FAST '25 | A + B | Artifact README: "3L Cache is implemented in the [libCacheSim] library, and its experimental environment configuration is consistent with libCacheSim"; repository layout notes "Forked from LibCacheSim, which is a platform for cache evaluation". The algorithm was subsequently upstreamed into this repository. | [optiq-lab/3L-Cache](https://github.com/optiq-lab/3L-Cache/tree/134cd159b635cdab75419a4281bed1a330fef31f), [USENIX](https://www.usenix.org/conference/fast25/presentation/zhou-wenbin), [issue #119](https://github.com/1a1a11a/libCacheSim/issues/119), [`3LCache/`](/libCacheSim/cache/eviction/3LCache/) |
| Man-Made Heuristics Are Dead. Long Live Code Generators! (PolicySmith) — Dwivedula, Saxena, Akella, Chaudhuri, Kim (UT Austin) | HotNets '25 | A + B | "Our prototype is built on libCacheSim, a high-performance web cache simulator with an event-driven interface." The artifact — "All code used for these case studies is available at https://github.com/ldos-project/policysmith" — wires in libCacheSim as a git submodule: `[submodule "webcache/libCacheSim"] … url = git@github.com:ldos-project/libcachesim.git`. Class A applies because the submodule points at a fork the same organisation maintains, not at upstream. | [ACM DL](https://doi.org/10.1145/3772356.3772413), [arXiv:2510.08803](https://arxiv.org/abs/2510.08803), [ldos-project/policysmith](https://github.com/ldos-project/policysmith/tree/6a0ee6dbd1cb388885b6be62995d54ef82bdd60e) |
| MetaMuse: Algorithm Generation via Creative Ideation — Ruiying Ma, Chieh-Jan Mike Liang, Yanjie Gao, Francis Y. Yan (Microsoft Research) | ICLR '26 | C | "For cache replacement, these n traces are generated by libCacheSim (Yang et al. 2020), from different Zipfian distributions." Used as the trace generator for its cache-replacement evaluation. | [ICLR proceedings](https://proceedings.iclr.cc/paper_files/paper/2026/hash/85632be2cd69e9a0ef4ba054c096fac9-Abstract-Conference.html), [arXiv:2510.03851](https://arxiv.org/abs/2510.03851) |
| T3-LRU: Three-Tier Hotness-Aware Concurrent Cache Eviction Algorithm — Zhang Xin et al. | Research Square preprint, 2026 | A | Evaluated in a fork of libCacheSim, archived as software: "rim99/libCacheSim: T3LRU Simulation … a high performance library for building cache simulators & T3-LRU simulation added". The preprint links both the fork and upstream libCacheSim. | [preprint](https://doi.org/10.21203/rs.3.rs-10049449/v1), [Zenodo](https://doi.org/10.5281/zenodo.20713182), [rim99/libCacheSim](https://github.com/rim99/libCacheSim/tree/b15718edf79b8fbf2e86256266f57e64289164f8) |
| Vulcan: Instance-specialized, Verifiable Systems Heuristics Through LLM-driven Search — Dwivedula, Saxena, Yadalam, Campbell, Kim, Akella | arXiv, Dec 2025 | B | "The scaffolding, implemented on top of `libCacheSim`, is responsible for instantiating the queues that are a part of the topology"; libCacheSim measures object hit rate for candidate heuristics. | [arXiv:2512.25065](https://arxiv.org/abs/2512.25065) |
| DynamicAdaptiveClimb: Adaptive Cache Replacement with Dynamic Resizing — Berend, Dolev, Kumari, Mishra, Kogan-Sadetsky, Somani | arXiv, Nov 2025 | C + A | "Simulator: We conduct all evaluations using libCacheSim [46], an open-source, high-performance, and extensible cache simulator widely adopted in recent caching research." Its related-work table also attributes SIEVE, 3L, ILRU, and both of its own policies to libCacheSim as the evaluation platform. The authors also publish a repository that vendors the simulator: "Integrated into the libCacheSim simulation framework", with `AdaptiveClimb.c` and `DynamicAdaptiveClimb.c` added under `libCacheSim/libCacheSim/cache/eviction/`. | [arXiv:2511.21235](https://arxiv.org/abs/2511.21235), [Dhruv27Mishra/Adaptive-Climb](https://github.com/Dhruv27Mishra/Adaptive-Climb/tree/becd5d148840b2910d4d983d9ed775500ecb857d) |
| FLOWS: Balanced MRC Profiling for Heterogeneous Object-Size Cache — Xiaojun Guo, Hua Wang, Ke Zhou (HUST), Hong Jiang (UT Arlington), Yaodong Han, Guangjie Xing (Tencent) | EuroSys '24 | B | "To simulate the cache environment, we employ libCachesim [3], a library that offers convenient I/O reading and cache instance functions. We have extended libCachesim to incorporate additional features such as cache size adjustment, MRC generation, and cache instance balancing." | [author copy](https://ranger.uta.edu/~jiang/publication/Conferences/2024/Eurosys24\(FLOWS\).pdf), [DOI](https://doi.org/10.1145/3627703.3650078) |
| ScaleOPT: A Scalable Optimal Page Replacement Policy Simulator — Hyung-Seok Han, Sangjin Lee, Yongseok Son (Chung-Ang University) | POMACS 8(3) / SIGMETRICS '25 | C | Benchmarked against libCacheSim as a baseline: "ScaleOPT improves the simulation time by up to 6.3×, 7.7×, 20.5×, and 13.9× compared with the long-established standard algorithm-based simulator along with our AccessMap, a variable-size cache scheme, and two widely-used cache simulators (webcachesim and libCacheSim), respectively." Quoted from the publisher-deposited abstract; the full text is paywalled. | [DOI](https://doi.org/10.1145/3700426), [Crossref abstract](https://api.crossref.org/works/10.1145/3700426) |
| Intent-Driven Storage Systems: From Low-Level Tuning to High-Level Understanding — Bergman, Song, Cavigelli, Berestizshevsky, Zhou, Zhang | arXiv, Oct 2025 | C | "We then evaluated all the traces for all policies using libcachesim (Yang et al. 2020), configured with a cache size of 0.1% of the working set." | [arXiv:2510.15917](https://arxiv.org/abs/2510.15917) |
| SCION: Size-aware Policy Orchestration for Nonstationary Object Caches — Qizhi Wang (PingCAP) | arXiv, 2026 | B | "We implement a trace-driven benchmark in C++ on top of libCacheSim [25]." The artifact contribution is stated as "We integrate DynamicAdaptiveClimb into libCacheSim, build a trace-conversion and evaluation pipeline for HR-Cache". Its prototype does not carry a copy of the source; it fetches upstream at a pinned commit and patches it, which is class B rather than A: "# libCacheSim (pinned to cacheMon/libCacheSim @ f7c85f8a538e62129e55e90c0ce4c0f605bd78ba)", with `patches/libcachesim-scion.patch` applied by `scripts/prepare_libcachesim.sh`. | [arXiv:2605.01055](https://arxiv.org/abs/2605.01055), [Icemap/SCION](https://github.com/Icemap/SCION/tree/3f13ac3e397d686708cae00d227e5caa2d226548) |

### 2.3 Independent cross-validation

| Who | What | Class | Evidence | Source |
|---|---|---|---|---|
| Ben Manes (maintainer, [Caffeine](https://github.com/ben-manes/caffeine)) | Ran libCacheSim side-by-side with Caffeine's simulator to re-check published S3-FIFO/SIEVE hit-ratio results, and reported a size-accounting discrepancy back to this project | C | "I used libcachesim at 0f4d135 (current master) … with this patch to include the new trace formats." | [issue #18](https://github.com/1a1a11a/libCacheSim/issues/18) |

This is the clearest instance of libCacheSim being used *adversarially* by a third party
— an outside maintainer reproducing the project's own numbers — which makes it worth
recording separately from citation-style use. Caffeine's simulator also ships dedicated
libCacheSim trace readers; see [derived implementations and trace-format interoperability](#derived-implementations-and-trace-format-interoperability).

### 2.4 Practitioner and community use

Use outside the publication record: industry engineers, and public reproducibility
artifacts that add an algorithm to libCacheSim without an accompanying paper.

| Who | What | Class | Evidence | Source |
|---|---|---|---|---|
| Marc Brooker — engineer at Amazon Web Services | Designed a SIEVE-k variant, evaluated it in libCacheSim against real-world traces, and published his implementation as a fork | C + A | "Using the excellent open source libCacheSim I tried SIEVE-2 against SIEVE on a range of real-world traces." and "I've implemented SIEVE-k in a fork of libCacheSim." The fork carries [`Sieve_k.c`](https://github.com/mbrooker/libCacheSim/blob/0147d1e93d411165492554a4e2cccbcb1d610fb0/libCacheSim/cache/eviction/Sieve_k.c) (10 KB) in the eviction directory. | [brooker.co.za](https://brooker.co.za/blog/2023/12/15/sieve.html), [mbrooker/libCacheSim](https://github.com/mbrooker/libCacheSim/tree/0147d1e93d411165492554a4e2cccbcb1d610fb0) |
| Bintang Dwi Marthen | Chameleon Cloud reproducibility artifact implementing CLOCK-Pro | B | "This artifact is my implementation of ClockPro on libCacheSim"; "Implemented on libCacheSim: a high performance library for building cache simulators" | [Trovi artifact](https://chameleoncloud.org/experiment/share/1a05c09b-f149-4555-b133-a4114155746b) |
| Raden Rafly Hanggaraksa Budiarto | Chameleon Cloud reproducibility artifact implementing CAR | B | "Implemented the CAR (Clock with Adaptive Replacement) cache implementation on LibCacheSim"; "Implemented on libCacheSim: a high performance library for building cache simulators." | [Trovi artifact](https://chameleoncloud.org/experiment/share/bac62a10-3868-4a77-9075-7e9247dd199b) |
| Muhammad Haekal Muhyidin Al-Araby | Chameleon Cloud reproducibility artifact demonstrating AdaptSize | C | "This artifact contain an example on how to run AdaptSize on LibCacheSim." | [Trovi artifact](https://chameleoncloud.org/experiment/share/f9cdc812-0c61-46ab-848d-295e47a4954c) |
| L. Stampf — BSc thesis, Vrije Universiteit Amsterdam, 2025 | Compared cache eviction primitives on Zipf-like synthetic workloads, and fixed the macOS build upstream while doing it | C | "The building and execution of cache simulations was performed using the cache simulator provided by libCacheSim, a high-performance open-source library specifically designed for cache simulation and trace analysis." The thesis documents the upstream PRs it produced; [#179](https://github.com/1a1a11a/libCacheSim/pull/179) and [#181](https://github.com/1a1a11a/libCacheSim/pull/181) were merged, [#183](https://github.com/1a1a11a/libCacheSim/pull/183) was not. | [thesis PDF](https://www.cs.vu.nl/~wanf/theses/stampf-bscthesis.pdf) |
| `gws8820` | Fork extending the simulator to two cache levels | A | README: "runs in 2-Level so can set different replacement algorithm and cache size for each level." | [gws8820/2-Level-libCacheSim](https://github.com/gws8820/2-Level-libCacheSim/tree/d93106ca12b6b7f9593ab1db7aaa5482c73356df) |

The three Chameleon artifacts were found by scanning all 460 public Trovi artifacts, and
are dated within ten days of each other in April 2025. Their affiliation to the project
differs by author, and is stated per row rather than assumed for the group:

- **Bintang Dwi Marthen** (Bandung Institute of Technology, per the artifact metadata) is
  a co-author on the OSDI '26 paper above, listed there at Harvard University and
  Institut Teknologi Bandung — **project-affiliated**.
- **Muhammad Haekal Muhyidin Al-Araby** (Sepuluh Nopember Institute of Technology) is a
  co-author on the PVLDB '26 paper above — **project-affiliated**.
- **Raden Rafly Hanggaraksa Budiarto** (Bandung Institute of Technology) has **no
  established link** to the project. He shares an institution with Marthen and published
  within ten days of him, which is suggestive of a shared course, but no source found for
  this edition demonstrates it. Counted as third-party, which is the default when no
  affiliation can be shown.

Marc Brooker, `gws8820`, and the VU Amsterdam thesis are unaffiliated and count as
third-party.

---

## 3. Distribution channels

These rows are **first-party reach, not third-party adoption** — the project publishing
itself. They are tabulated because availability is what adoption elsewhere depends on,
and because release cadence and download volume are the only quantitative series this
census can track over time. The counts in the [changelog](#changelog) keep them separate
from the adoption entries in [§2](#2-research-artifacts-and-publications) and
[§4](#4-ecosystem-projects-and-datasets).

| Channel | Package | Class | State on 2026-08-13 | Source |
|---|---|---|---|---|
| PyPI | `libcachesim` (Python bindings, maintained under the cacheMon org) | D | Latest `0.3.3.post4` (2026-02-17); first release `0.3.2` (2025-07-14); 5 releases; requires Python ≥ 3.10; GPLv3+ | [PyPI](https://pypi.org/project/libcachesim/), [cacheMon/libCacheSim-python](https://github.com/cacheMon/libCacheSim-python/tree/7079d279873dff1052ad716670e7a49841f9d1e4) |
| npm | `libcachesim-node` (Node.js bindings, in-tree at [`libCacheSim-node/`](/libCacheSim-node)) | D | Latest `0.3.2` (2025-07-14); first publish `1.0.0` (2025-06-18); MIT | [npm](https://www.npmjs.com/package/libcachesim-node) |
| GitHub Releases | source releases | D | 5 releases, `v0.1` → `v0.3.5` (2026-03-15) | [Releases](https://github.com/1a1a11a/libCacheSim/releases) |

**Download volume.** PyPI, via [pypistats.org](https://pypistats.org/packages/libcachesim)
with `mirrors=false`: 1,805 downloads over the 2026-02-12 → 2026-08-11 window the API
exposes, 258 of them in the trailing 30 days. npm `libcachesim-node`: 173 downloads over 2025-08-10 → 2026-08-09
([fixed-window npm query](https://api.npmjs.org/downloads/range/2025-08-10:2026-08-09/libcachesim-node)).

**No third-party redistribution exists.** Every package of libCacheSim is published by
the project itself. Checked and found nothing: AUR, conda-forge/anaconda.org, vcpkg,
conan-center-index, Debian, Homebrew, Nix, and crates.io carry no libCacheSim port, and
Docker Hub returns one image, the project's own. deps.dev and GitHub's dependency graph
report zero reverse dependencies for the PyPI package. Whatever reach the library has, it
is not through downstream packaging.

The two figures differ in how checkable they are, and the difference is worth stating
rather than glossing.

The **npm** query is pinned to an explicit date range rather than a rolling one such as
`last-year`, so re-running that link reproduces this edition's number exactly.

The **PyPI** figure is not reproducible and cannot be made so. pypistats offers no
fixed-range endpoint; it serves a rolling window roughly 180 days wide, which drops its
earliest days as it advances. By the time you read this the 2026-02-12 → 2026-08-11
interval will have partly aged out of the API, so summing the stated dates will not
return 1,805. Treat it as a **point-in-time observation recorded on the census date**,
not as a verifiable claim. Committing the raw response alongside this file would preserve
provenance but would not make the number independently checkable — a reader would be
trusting this project's own copy — so it is left out and the limitation stated instead.
A future edition can compare like for like only by re-reading the same endpoint on its
own census date and recording the window it saw.

The two figures are not filtered alike, so do not add them or compare them directly. The
PyPI figure excludes the mirrors pypistats recognizes; the npm endpoint applies no mirror
filter at all. Neither excludes CI, container builds, or other automated installs, which
on a research library plausibly dominate. Treat both as a weak proxy recorded for
trend-tracking across editions, never as a user count.

---

## 4. Ecosystem projects and datasets

| Project | Class | Evidence | Source |
|---|---|---|---|
| **CacheBench** — benchmarking suite evaluating 18 eviction algorithms across thousands of traces, developed by Haocheng Xia (UIUC, visiting Harvard) under the UCSC OSPO Summer of Reproducibility | B | Report describes libCacheSim as a core component and the project as "a Python package that allows users to easily download traces and run simulation analyses using libCacheSim" | [UCSC OSPO report](https://ucsc-ospo.github.io/report/osre25/harvard/cachebench/2025-08-06-haochengxia/) |
| **system-intelligence-benchmark** (`sys-intelligence` org) — benchmark suite scoring LLM-designed systems heuristics, whose `cache_algo_bench` task scores candidate eviction policies against libCacheSim's algorithm set | C | Shells out to the built binary: `command = f"""{LIBCACHSIM_PATH}/_build/bin/cachesim {cache_trace} oracleGeneral {cache_alg} {cache_cap} --ignore-obj-size 1 …"""`, and its README calls libCacheSim a "high-performance caceh simulator" [sic] | [sys-intelligence/system-intelligence-benchmark](https://github.com/sys-intelligence/system-intelligence-benchmark/tree/46596edd8113a3eaf5646e49a42cc2a9fae3de4d) |
| **cache_dataset** — open collection of production cache traces (Meta, Twitter, CloudPhysics, Microsoft, Wikimedia, Alibaba, Tencent) | C | Ships three tutorial notebooks that run libCacheSim — "Using libCacheSim to read the dataset", "…to analyze and plot the trace", "…to run cache simulation". The dataset itself is *format-compatible* rather than built on the library: "We provide both plain text format … and `oracleGeneral` format that is suitable for using with [libCacheSim] platform." | [cacheMon/cache_dataset](https://github.com/cacheMon/cache_dataset/tree/a005343f26f47110de5c8d78d645ee89bee1e7ed) |

Note the class distinction between these two rows: CacheBench is implemented on top of
libCacheSim (B), whereas cache_dataset publishes traces in a libCacheSim-readable format
and drives the tool from tutorials (C). Shipping a compatible format alone would not
qualify for either class.

Affiliation differs by row. cache_dataset is published under the same cacheMon
organization that maintains the Python bindings, and CacheBench was built by a
libCacheSim collaborator under project mentorship, so both are **project-affiliated** and
excluded from the third-party subtotal in the [changelog](#changelog).
system-intelligence-benchmark is an unaffiliated third-party suite and counts as such.
All three are adoption entries — the software is genuinely used.

### Derived implementations and trace-format interoperability

Work that takes something *from* libCacheSim — its algorithm code or its trace format —
without running, bundling, or building on the software. None of it satisfies classes A–D,
so **none of these rows count toward any total above**. They are recorded because they
show a second kind of reach: the implementations and the `oracleGeneral` format
travelling on their own.

| Project | What it takes | Evidence | Source |
|---|---|---|---|
| **Cache is King: Smart Page Eviction with eBPF** — Zussman, Zarkadas, Carin, Cheng, Franke, Pfefferle, Cidon (Columbia; IBM Research) | The LHD implementation, ported to eBPF | "We implement LHD using cachebpf, based on the implementation in libcachesim [69, 70, 72]." The paper mentions libCacheSim once and shows no run of it, so this is derivation from the source rather than use of the tool. | [arXiv:2502.02750](https://arxiv.org/abs/2502.02750) |
| **Pelikan** `cachesim` (Rust) | The binary trace formats | "A cache trace simulator … with a trace format inspired by [libCacheSim]"; "cachesim can import libCacheSim's binary trace formats"; "The first four columns correspond directly to libCacheSim's **oracleGeneral** binary format"; "The `op` column uses the same integer encoding as libCacheSim's `req_op_e`" | [pelikan-io/cachesim](https://github.com/pelikan-io/cachesim/tree/9997462b2b3746fbd826837702027afbdea57d7a) |
| **Otter** (Go cache library) | The trace formats | Its benchmark simulator carries a `libcachesim` parser package and offers the formats as inputs: `OracleGeneralFormat = "oracleGeneral"`, `LibcachesimCSVFormat = "libcachesimCSV"` | [maypok86/otter](https://github.com/maypok86/otter/tree/8c526307556486ea0337280a4211135720bc29cc) |
| **Caffeine** (Java cache library) | The trace formats | Its simulator registers three libCacheSim readers — `LCS_TRACE`, `LCS_ORACLE_GENERAL`, `LCS_TWITTER` — backed by a `parser/libcachesim/{csv,oracle,twitter}` package tree | [ben-manes/caffeine](https://github.com/ben-manes/caffeine/tree/9da6581ee366aa63c51e0dc96692d02f9c29ccff) |

Otter and Caffeine also implement SIEVE or S3-FIFO, which is
[algorithm adoption](#6-downstream-algorithm-adoption-not-libcachesim-adoption) and
counted nowhere here either; their rows above are strictly about the trace format.

---

## 5. Repository signals

Aggregate signals from the GitHub API on 2026-08-13. These measure attention and
contribution, not deployment, and are listed apart from sourced adoption for that reason.

| Signal | Value |
|---|---|
| Stars | 339 |
| Forks | 111 |
| Contributors (non-anonymous) | 36 |
| Open issues (excluding pull requests) | 21 |
| Open pull requests | 8 |
| Created | 2020-06-19 |
| License | GPL-3.0 |

The two counts are split deliberately. The repository endpoint's `open_issues_count`
field — 29 at census time — is **not** an issue count: GitHub counts pull requests as
issues there, so the field is the sum of the two rows above. Reading it as "open issues"
would overstate the backlog and corrupt trend comparisons between editions. The 8 open
pull requests include the one that added this document.

Forks were enumerated and their READMEs fetched; they are *not* treated as adoption.
Almost all are dormant snapshots of upstream text, including several with distinctive
names that turn out to carry no divergent work. The forks that do carry independent work
are listed by name in [§2.2](#22-third-party) and
[§2.4](#24-practitioner-and-community-use) instead — a fork click is not adoption, a fork
with a new eviction algorithm in it is.

---

## 6. Downstream algorithm adoption (not libCacheSim adoption)

SIEVE (NSDI '24) and S3-FIFO (SOSP '23) were developed and evaluated with libCacheSim,
and are now reimplemented in third-party systems. **These systems do not use
libCacheSim.** They are recorded because they are the research output's reach, and
conflating the two is the most likely way this census could be misread.

Spot-verified for this edition:

| System | Evidence | Source |
|---|---|---|
| Ceph | `src/common/web_cache.h` header comment: "The implementation is based on SIEVE [0] with additional TTL", citing the NSDI '24 paper directly; contains `SieveQueue`, `_sieve_hand`, `sieve_evict()` | [ceph/ceph](https://github.com/ceph/ceph/blob/5995d21863b3992bd9f463b5a0f774869351be36/src/common/web_cache.h) |
| TiDB | `pkg/infoschema/sieve.go` implements a SIEVE cache (per-entry `visited` flag) | [pingcap/tidb](https://github.com/pingcap/tidb/blob/d5f9ca5690c0a53cac36002f9d2d2bdcba25f4fc/pkg/infoschema/sieve.go) |

A broader list — immudb, DragonFly, dnscrypt-proxy, PostgREST, Pelikan, SkiftOS,
Nyrkiö, plus 20+ language-level cache libraries — is maintained on the
[SIEVE project site](https://cachemon.github.io/SIEVE-website/). That list is
project-maintained and partly self-reported; apart from the two rows above, its entries
were **not** independently verified for this edition.

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
| 1.0.0 | 2026-08-13 | First edition. **28 adoption entries** — 8 first-party artifacts, 10 third-party works, 1 independent cross-validation, 6 practitioner and community entries, and 3 ecosystem projects — of which **16 are third-party**. Plus 3 first-party distribution channels, which are reach rather than adoption, for **31 classed rows**. By evidence class: 11×A, 9×B, 13×C, 3×D (36 assignments over 31 rows; five rows carry two classes). Recorded but counted nowhere: the OSDI '20 row that introduced the simulator, 4 derived-implementation and trace-format rows, repository signals, and downstream algorithm adoption. |
