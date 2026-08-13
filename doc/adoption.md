# libCacheSim Adoption Census

A source-linked inventory of who, outside this project, uses libCacheSim. Every entry quotes
the sentence in a primary source that supports it and links to that source. Entries without a
checkable source do not appear.

| | |
|---|---|
| **Census version** | `1.2.0` |
| **Census date** | 2026-08-13 |
| **Repository snapshot** | `develop` @ [`dc80ebf`](https://github.com/1a1a11a/libCacheSim/commit/dc80ebf695c54601f9d00f815a730e080186a23f); latest release [`v0.3.5`](https://github.com/1a1a11a/libCacheSim/releases/tag/v0.3.5) (2026-03-15) |
| **Canonical location** | [`doc/adoption.md`](https://github.com/1a1a11a/libCacheSim/blob/develop/doc/adoption.md) |

**Scope.** A work is listed if a primary source shows it builds on, bundles, forks,
distributes, or runs libCacheSim. Quotations were matched against raw source text — not a
summary — and every GitHub link is pinned to a commit so a later upstream edit cannot
strand a quotation. The only alterations made to a quotation are closing up line breaks where
the source wraps a sentence across two lines of a comment or a PDF, and marking omitted words
with an ellipsis.

**First-party work is excluded.** libCacheSim's own papers and artifacts — the OSDI '20
paper that introduced the simulator, GL-Cache, S3-FIFO, SIEVE, QD-LP, S4-FIFO, Lazy
Promotion, Clock2Q+, and Juncheng Yang's dissertation — are not entries here: they are the
project, not evidence of its reach. The same rule excludes artifacts, forks, and ecosystem
projects authored by the project's own members and co-authors, and the student work from the
maintainer's own course ([§2](#2-third-party-forks)). Every entry below is third-party.

**Not adoption.** SIEVE and S3-FIFO were designed and evaluated with libCacheSim and are
now reimplemented in many third-party systems. Those systems are not libCacheSim users.
They, and work that borrows only the trace format or an algorithm implementation, are
recorded in [§5](#5-related-but-not-adoption) and counted separately.

---

## 1. Third-party research

| Work | Venue | Evidence | Source |
|---|---|---|---|
| Writeback Modeling: Theory and Application to Zipfian Workloads — Smith, Ding (Rochester), Byrne (Michigan Tech) | MEMSYS '21 | "The simulator is based on libCacheSim [36] written in C++ compiled with g++ 5.4 (-O3)." The earliest third-party use in this edition. | [PDF](https://www.memsys.io/wp-content/uploads/2023/09/p2.pdf), [DOI](https://doi.org/10.1145/3488423.3519331) |
| FLOWS: Balanced MRC Profiling for Heterogeneous Object-Size Cache — Guo, Wang, Zhou (HUST), Jiang (UT Arlington), Han, Xing (Tencent) | EuroSys '24 | "To simulate the cache environment, we employ libCachesim [3] … We have extended libCachesim to incorporate additional features such as cache size adjustment, MRC generation, and cache instance balancing." Its first author later upstreamed that capability as `mrcProfiler` ([PR #136](https://github.com/1a1a11a/libCacheSim/pull/136), merged, +2,488 lines), writing "I have tested `mrcProfiler` with twitter trace `cluster52.oracleGeneral.sample10`, and the results are as follows:" | [author copy](https://ranger.uta.edu/~jiang/publication/Conferences/2024/Eurosys24\(FLOWS\).pdf), [DOI](https://doi.org/10.1145/3627703.3650078) |
| ScaleOPT: A Scalable Optimal Page Replacement Policy Simulator — Han, Lee, Son (Chung-Ang University) | POMACS 8(3) / SIGMETRICS '25 | Benchmarked against it: "ScaleOPT improves the simulation time by up to 6.3×, 7.7×, 20.5×, and 13.9× compared with … two widely-used cache simulators (webcachesim and libCacheSim)." From the publisher-deposited abstract; full text paywalled. | [DOI](https://doi.org/10.1145/3700426), [Crossref](https://api.crossref.org/works/10.1145/3700426) |
| Incremental Least-Recently-Used Algorithm: Good, Robust, and Predictable Performance — Zhang, Chen, Cai (Sun Yat-sen University), Lui (CUHK) | IEEE TMC 24(7), 2025 | "Our trace-driven simulations utilize libCacheSim[54]," — file caching for mobile edge computing, with reference [54] pointing at this repository: "Libcachesim: A high-performance library for building cache simulators". No artifact was released. Paywalled at IEEE; verified from the author copy. | [author copy](https://www.cse.cuhk.edu.hk/~cslui/PUBLICATION/Incremental_Least-Recently-Used_Algorithm_Good_Robust_and_Predictable_Performance.pdf), [DOI](https://doi.org/10.1109/TMC.2025.3547066) |
| CAPSULE: A Storage Prefetcher Harnessing Spatio-Temporal Locality for Cloud-Scale Workloads — Ramadhan, Yoo, Choi (Dankook University) | POMACS 10(1) / SIGMETRICS '26 | Artifact record: "This work extends the libCacheSim framework, which is also licensed under GPLv3." Its tree is laid out as "libCacheSim/            # Cache simulator core and CAPSULE code", and its baselines are "prefetching schemes (PG, OBL, and Mithril), all integrated into the libCacheSim simulator" — the prefetch module an outside contributor added upstream ([§3](#contributed-from-outside-the-project)). The Zenodo record has since been withdrawn (HTTP 410); the deposited description survives at DataCite. | [DOI](https://doi.org/10.1145/3788088), [artifact DOI](https://doi.org/10.5281/zenodo.18136656), [DataCite record](https://api.datacite.org/dois/10.5281/zenodo.18136656) |
| Linear Elastic Caching via Ski Rental — Kumar, Lipcon, Purohit, Sarlos (Google) | CIDR '25 | "We implemented our ski rental based algorithms in libCacheSim [1]." The resulting policy — not libCacheSim itself — is reported as deployed in Spanner. | [CIDR](https://mail.vldb.org/cidrdb/papers/2025/p22-kumar.pdf) |
| 3L-Cache: Low Overhead and Precise Learning-based Eviction Policy for Caches — Zhou, Niu, Xiong, Fang, Wang (BJUT; Microsoft Research) | FAST '25 | "3L Cache is implemented in the [libCacheSim] library"; the repository is "Forked from LibCacheSim, which is a platform for cache evaluation". The algorithm was later upstreamed here. A second, anonymised copy exists for double-blind review, carrying the same sentence. | [optiq-lab/3L-Cache](https://github.com/optiq-lab/3L-Cache/tree/134cd159b635cdab75419a4281bed1a330fef31f), [USENIX](https://www.usenix.org/conference/fast25/presentation/zhou-wenbin), [issue #119](https://github.com/1a1a11a/libCacheSim/issues/119), [`3LCache/`](/libCacheSim/cache/eviction/3LCache/), [admin333-paper/3L-Cache](https://github.com/admin333-paper/3L-Cache/tree/20174ef2bc219f0661ba6cddb506436e61bc4168) |
| Merlin: An Efficient Adaptive Cache Eviction Algorithm via Fine-Grained Characterization — Li, Guo, Fan, Wu, Zhang, Wang, Luo, Zhou (Peking University), Wang (Michigan Tech), Tamir (UCLA) | OSDI '26 | "We implemented Merlin in both CacheLib [11] … and libCacheSim [2]"; hit rates are evaluated in libCacheSim and flash-friendliness "on an extension of libCacheSim". The authors state they "contributed … an implementation of CAR to libCacheSim" — their claim; [`CAR.c`](/libCacheSim/cache/eviction/CAR.c) ships here, but the merged CAR contribution in this repository's tracker is [PR #131](https://github.com/1a1a11a/libCacheSim/pull/131), by someone else. | [USENIX](https://www.usenix.org/conference/osdi26/presentation/li-liujia) |
| Man-Made Heuristics Are Dead. Long Live Code Generators! (PolicySmith) — Dwivedula, Saxena, Akella, Chaudhuri, Kim (UT Austin) | HotNets '25 | "Our prototype is built on libCacheSim, a high-performance web cache simulator with an event-driven interface." Its artifact wires in a fork the same organisation maintains: `[submodule "webcache/libCacheSim"] … url = git@github.com:ldos-project/libcachesim.git`. Three lab members' personal forks carry the LLM-in-the-loop scaffold, e.g. "// This function is a placeholder for the LLM generated code." | [ACM DL](https://doi.org/10.1145/3772356.3772413), [arXiv:2510.08803](https://arxiv.org/abs/2510.08803), [ldos-project/policysmith](https://github.com/ldos-project/policysmith/tree/6a0ee6dbd1cb388885b6be62995d54ef82bdd60e), [DivyanshuSaxena/libCacheSim](https://github.com/DivyanshuSaxena/libCacheSim/tree/fd7b874a912a020f03553141abc1e141b69587a8) |
| Vulcan: Instance-specialized, Verifiable Systems Heuristics Through LLM-driven Search — Dwivedula, Saxena, Yadalam, Campbell, Kim, Akella (UT Austin) | arXiv, Dec 2025 | "The scaffolding, implemented on top of `libCacheSim`, is responsible for instantiating the queues that are a part of the topology." libCacheSim is the search loop's evaluator: "we opted to use a simulator – libcachesim [45] – which runs the newly generated heuristic and measures the object hit rate". No artifact repository has been published. | [arXiv:2512.25065](https://arxiv.org/abs/2512.25065) |
| MetaMuse: Algorithm Generation via Creative Ideation — Ma, Liang, Gao, Yan (Microsoft Research) | ICLR '26 | "For cache replacement, these n traces are generated by libCacheSim (Yang et al. 2020), from different Zipfian distributions." Used as the trace generator. | [ICLR](https://proceedings.iclr.cc/paper_files/paper/2026/hash/85632be2cd69e9a0ef4ba054c096fac9-Abstract-Conference.html), [arXiv:2510.03851](https://arxiv.org/abs/2510.03851) |
| Intent-Driven Storage Systems: From Low-Level Tuning to High-Level Understanding — Bergman, Song, Cavigelli, Berestizshevsky, Zhou, Zhang | arXiv, Oct 2025 | "We then evaluated all the traces for all policies using libcachesim (Yang et al. 2020), configured with a cache size of 0.1% of the working set." | [arXiv:2510.15917](https://arxiv.org/abs/2510.15917) |
| DynamicAdaptiveClimb: Adaptive Cache Replacement with Dynamic Resizing — Berend, Dolev, Kogan-Sadetsky (Ben-Gurion), Kumari, Mishra, Somani (Shiv Nadar) | arXiv, Nov 2025 | "Simulator: We conduct all evaluations using libCacheSim [46], an open-source, high-performance, and extensible cache simulator widely adopted in recent caching research." The authors' repository vendors the simulator and adds `AdaptiveClimb.c` and `DynamicAdaptiveClimb.c` under its eviction directory. Its bibliography misattributes libCacheSim to other authors at a repository that does not exist, so citation-graph searches miss this paper — one reason this census is built from full text and code rather than reference lists. | [arXiv:2511.21235](https://arxiv.org/abs/2511.21235), [Dhruv27Mishra/Adaptive-Climb](https://github.com/Dhruv27Mishra/Adaptive-Climb/tree/becd5d148840b2910d4d983d9ed775500ecb857d) |
| SCION: Size-aware Policy Orchestration for Nonstationary Object Caches — Qizhi Wang (PingCAP) | arXiv, 2026 | "We implement a trace-driven benchmark in C++ on top of libCacheSim [25]." Its prototype fetches upstream at a pinned commit and applies `patches/libcachesim-scion.patch`. | [arXiv:2605.01055](https://arxiv.org/abs/2605.01055), [Icemap/SCION](https://github.com/Icemap/SCION/tree/3f13ac3e397d686708cae00d227e5caa2d226548) |
| T3-LRU: Three-Tier Hotness-Aware Concurrent Cache Eviction Algorithm — Zhang Xin et al. | Research Square preprint, 2026 | Evaluated in a fork archived as software: "rim99/libCacheSim: T3LRU Simulation … a high performance library for building cache simulators & T3-LRU simulation added". | [preprint](https://doi.org/10.21203/rs.3.rs-10049449/v1), [Zenodo](https://doi.org/10.5281/zenodo.20713182), [rim99/libCacheSim](https://github.com/rim99/libCacheSim/tree/b15718edf79b8fbf2e86256266f57e64289164f8) |

---

## 2. Third-party forks

Work that lives in a fork rather than in a paper. Every fork below was checked against
upstream's own object graph, so a branch that merely copies an upstream branch does not
appear; each row's commits are the owner's own.

| Fork | Who | What it adds | Evidence | Source |
|---|---|---|---|---|
| `fedorova/libCacheSim` | Alexandra (Sasha) Fedorova — MongoDB / UBC | An emulation of WiredTiger's eviction algorithm: `WiredTiger.c` with read-generation buckets, leaf/internal page sets and an on-demand btree walk, plus WT page fields threaded through the core structures | "An emulation of the WiredTiger eviction algorithm for the in-memory cache." The 106 commits on the branch are authored `sasha.fedorova@mongodb.com`. She also filed four upstream issues while doing it, publishing her WiredTiger trace: "Here is the entire gzipped trace: https://people.ece.ubc.ca/~sasha/TMP/evict-btree.csv.gz" | [`WiredTiger.c`](https://github.com/fedorova/libCacheSim/blob/c90ef9a8c388bed3bb68a690a5d1a1959075590f/libCacheSim/cache/eviction/WiredTiger.c), [issue #29](https://github.com/1a1a11a/libCacheSim/issues/29) |
| `ddkkpp/libCacheSim` | `dingkp` — University of Science and Technology of China | LOH, a reinforcement-learning eviction policy with a CMA-ES bridge: `LOH.c` plus penalty/teacher variants, `actor_critic_functions.c`, ~80 dated design notes and robustness plots against 3L-Cache | "LOH (Learning-based Object Handling) 是一个结合传统启发式算法和强化学习的缓存驱逐算法。" Commits are authored `dingkp@mail.ustc.edu.cn`; a deleted `CDN_paper/` directory indicates an unpublished paper. The same person filed five bug reports here on Meta and Wikipedia traces and fixed one, merged as [PR #251](https://github.com/1a1a11a/libCacheSim/pull/251) | [design note](https://github.com/ddkkpp/libCacheSim/blob/8924f009e0f852f42c8e8d6a3e8c2754f97d8254/docs/20250814-LOH_Algorithm_Architecture.md), [issue #192](https://github.com/1a1a11a/libCacheSim/issues/192) |
| `access-bits/libCacheSim` | Arunkrishna Annai Madalam Sivasubramanian — EPFL | TLB and access-bit filtering for memory tiering: four `*TlbFiltered` policies, four new oracle readers, a YAML configuration subsystem with 20 workload configs, and single-reader/multi-worker parallelism | "Type 2: Belady evicts a page that is currently resident in some TLB" Commits are authored from an EPFL cluster host, `annai@iccluster101.iccluster.epfl.ch` | [`BeladyLruTlbFiltered.c`](https://github.com/access-bits/libCacheSim/blob/d8b8f939731783344683d8371068e309d22ed7bf/libCacheSim/cache/eviction/BeladyLruTlbFiltered.c) |
| `tzussman/libCacheSim` | Tal Zussman — Columbia University | RingLFU, a bucketed frequency policy, with a 17 KB design spec | "There are no per-item frequency counters. The bucket position *is* the frequency estimate. Per-item eviction metadata is 1 byte." Authored `tz2294@columbia.edu`. Zussman is also first author of Cache is King ([§5](#5-related-but-not-adoption)), which ports an implementation out of libCacheSim without reporting a run of it — this fork is the group's actual use | [`ring-lfu.md`](https://github.com/tzussman/libCacheSim/blob/9a0f7b99f10431511a7f2ec3862e7eecced5d3e1/ring-lfu.md) |
| `shermanjlim/libCacheSim` | Sherman Lim — Carnegie Mellon University | A DRAM-plus-flash hybrid cache (`HYBRID.cpp`), a MAGIC policy, a future-access admission policy, and two runnable examples | `bool is_m(const request_t *req) { return req->features[ISM_FEATURE_IDX] == 1; }` — the fork drives libCacheSim's feature-carrying `lcs` traces, and two of his upstream fixes to that path were merged: "I think there's a typo in the conditionals when checking if we can support lcs trace for the Belady algo" | [`HYBRID.cpp`](https://github.com/shermanjlim/libCacheSim/blob/0454caa896c0c24105f3e362804a32b291190697/libCacheSim/cache/eviction/cpp/HYBRID.cpp), [PR #292](https://github.com/1a1a11a/libCacheSim/pull/292) |
| `ouyhlan/libCacheSim` | `ouyhlan` | ZGCache and ZCCache, learned caches built on LightGBM and XGBoost, plus segmented ARC and FIFO variants | `#include <LightGBM/c_api.h>` … `#define N_MAX_TRAINING_DATA 8000` — four self-contained algorithm directories delivered as a single commit named `final-version`, alongside trace statistics for a Meta workload | [`ZGCache.hpp`](https://github.com/ouyhlan/libCacheSim/blob/f365920c12204101516883971de53a75a8d41785/libCacheSim/cache/eviction/ZGCache/ZGCache.hpp) |
| `Aryan470/libCacheSim` | Aryan Khatri | Fourteen embedding-similarity policies on a shared `EmbeddingManager`, with nine parameter-sensitivity and regret-analysis scripts | "LRU with Forgiveness: A modified LRU that uses embedding-based forgiveness to protect objects with high similarity to recent accesses from eviction." | [`LRUForgive.cpp`](https://github.com/Aryan470/libCacheSim/blob/3f459ffbcbdb42f5218172f74bd08949f71b821e/libCacheSim/cache/eviction/LRUForgive.cpp) |
| `varungohil/libCacheSim` | Varun Gohil | Instrumentation for prefetcher/eviction interference, with a documented experiment and a cache-size sweep | "**Prefetch → evict:** a prefetched object is evicted before any demand hit (wasted prefetch / cache pollution)." Commits come from a CloudLab node whose profile is named `libcachesim`; this is the most recently active fork found | [experiment doc](https://github.com/varungohil/libCacheSim/blob/24c0d6391a526bd116b203da33cd6c49fcf7b489/doc/prefetch_interaction_experiment.md) |
| `midsterx/libCacheSim` | `midsterx` | Cost sweeps over IBM Object Storage traces on a branch named `macaron_experiments`, with the simulator modified to emit `gib_missed` and `cost` | "3. We assume that libCacheSim results do not differ between running all eviction policies together vs. running them separately" and, in the setup notes, "1. Setup xgboost, install dependencies, and build libCacheSim". He also got a defaulting bug fixed upstream, having written "I am currently using this library to understand the eviction algorithms that I should be exploring." | [`myResults/README.txt`](https://github.com/midsterx/libCacheSim/blob/57d71f9147b56a6d7657199e72b90829f11139bb/myResults/README.txt), [issue #25](https://github.com/1a1a11a/libCacheSim/issues/25) |
| `LauYeeYu/libCacheSim` | Yiyu Liu | Compute-aware eviction (`BeladyCompute.c`, `S3FIFOCompute.c`, `GDSF_compute.cpp`), an LHD variant family, and an LLM-trace reader | "LHDRequest is a new version of LHD Compute that operates on LLM request level instead of block level." He also fixed a TwoQ assertion failure upstream: "when Am is empty, an assertion failure will be encountered" | [`README_LHDRequest.md`](https://github.com/LauYeeYu/libCacheSim/blob/bb1c323989d6f49421b95c4a241c2ba755f70612/libCacheSim/cache/eviction/LHD/README_LHDRequest.md), [PR #295](https://github.com/1a1a11a/libCacheSim/pull/295) |
| `gws8820/2-Level-libCacheSim` | `gws8820` | A two-level cache hierarchy with independent policies and sizes per level | README: "runs in 2-Level so can set different replacement algorithm and cache size for each level." It carries the full source tree but is a re-upload rather than a GitHub fork, so it is absent from the fork network and from the arithmetic in [§6](#6-repository-signals) | [gws8820/2-Level-libCacheSim](https://github.com/gws8820/2-Level-libCacheSim/tree/d93106ca12b6b7f9593ab1db7aaa5482c73356df) |

Forks belonging to entries listed elsewhere are not repeated here: `optiq-lab/3L-Cache`,
`Icemap/SCION`, `Dhruv27Mishra/Adaptive-Climb` and `rim99/libCacheSim` are counted with
their papers in [§1](#1-third-party-research), and `mbrooker/libCacheSim` with its author in
[§3](#3-practitioner-and-community-use).

**Smaller forks, not counted individually.** A Yale researcher's Docker/`just` packaging
(`shsym`), a Columbia researcher's 2021 work on the `exec` driver (`yuhong-zhong`), sampled
LRU and SIEVE variants (`huyk18`), a WATT policy from FAU Erlangen (`itodnerd`), a 3L-Cache
model ablation (`gaurav-2408`), and a hand-rolled pybind11 wrapper from Princeton
(`0austinli4`) that upstream's own bindings later superseded.

**Excluded: the maintainer's own course.** Seventeen forks carry a `cs2640` branch of
student cache-competition projects — plugin-API policies, tuning sweeps, and in one case a
report reading "Does replacing S3-FIFO's hardcoded $S \to M$ promotion rule with a tiny
online linear classifier deliver reliable miss-ratio improvements across heterogeneous cache
workloads?" ([Minkai25/caching_competition](https://github.com/Minkai25/caching_competition/tree/3bdf0b56bb03bf46444bd3c80607b7e1ff24fe4f)).
The course — "CS2640 Modern (Computer) Storage Systems" — names the project's maintainer as
its instructor, and its [competition page](https://moderncomputerstorage.com/competition)
tells students to "Review the libCacheSim plugin guide and examples to learn the expected
interface". Commit hostnames place the work on Harvard's cluster and on CloudLab's
`cs2640-pg0` project. This is the project teaching with its own tool, so it is first-party by
the rule above — recorded because seventeen forks is the largest single cluster in the fork
graph, and a reader counting forks will otherwise miscount it.

---

## 3. Practitioner and community use

Use outside the publication record.

| Who | What | Evidence | Source |
|---|---|---|---|
| Ben Manes — maintainer of [Caffeine](https://github.com/ben-manes/caffeine) | Ran libCacheSim beside Caffeine's own simulator to re-check published S3-FIFO/SIEVE results, and reported a size-accounting discrepancy back to this project | "I used libcachesim at 0f4d135 (current master) … with this patch to include the new trace formats." | [issue #18](https://github.com/1a1a11a/libCacheSim/issues/18) |
| Marc Brooker — engineer at Amazon Web Services | Designed a SIEVE-k variant, evaluated it here, and published his implementation as a fork | "Using the excellent open source libCacheSim I tried SIEVE-2 against SIEVE on a range of real-world traces"; "I've implemented SIEVE-k in a fork of libCacheSim." The fork carries [`Sieve_k.c`](https://github.com/mbrooker/libCacheSim/blob/0147d1e93d411165492554a4e2cccbcb1d610fb0/libCacheSim/cache/eviction/Sieve_k.c). | [brooker.co.za](https://brooker.co.za/blog/2023/12/15/sieve.html), [mbrooker/libCacheSim](https://github.com/mbrooker/libCacheSim/tree/0147d1e93d411165492554a4e2cccbcb1d610fb0) |
| The SOSP '23 Artifact Evaluation committee | Installed and ran libCacheSim to validate a submitted artifact, independently of its authors, on five different machines | Review summary, under step-by-step instructions: "Cloned and installed libCacheSim from [https://github.com/1a1a11a/libCacheSim](https://github.com/1a1a11a/libCacheSim)" | [sysartifacts.github.io](https://github.com/sysartifacts/sysartifacts.github.io/blob/89cc8991c2751fd311bbdd23fa16c15b060d207e/_conferences/sosp2023/summaries/fifo.md) |
| L. Stampf — BSc thesis, Vrije Universiteit Amsterdam, 2025 | Compared eviction primitives on Zipf-like workloads, and fixed the macOS build upstream while doing it | "The building and execution of cache simulations was performed using the cache simulator provided by libCacheSim." Her code is published too: "The code started with a modified version of the libCacheSim codebase, but was further tinkered with to fit the needs of this work." Upstream PRs [#179](https://github.com/1a1a11a/libCacheSim/pull/179) and [#181](https://github.com/1a1a11a/libCacheSim/pull/181) were merged; [#183](https://github.com/1a1a11a/libCacheSim/pull/183) was not. | [thesis PDF](https://www.cs.vu.nl/~wanf/theses/stampf-bscthesis.pdf), [laustam/cache-eviction-thesis](https://github.com/laustam/cache-eviction-thesis/tree/ab51dabbdf8979e7b27e19329b6565fcddf1856b) |
| Rodrigo Caridad — University of Chicago | Chameleon Cloud artifact reproducing part of the SIEVE paper on bare metal, against FIFO, LRU, CLOCK and ARC baselines | The notebook builds the simulator on the provisioned node: `my_server.execute("cd NSDI24-SIEVE/libCacheSim/scripts && bash install_libcachesim.sh")`, and its recorded output shows the resulting binary | [Trovi](https://chameleoncloud.org/experiment/share/36015dec-c76a-4d25-a85f-b583a97bd036), [RorroArt/sieve-chameleon-repro](https://github.com/RorroArt/sieve-chameleon-repro/tree/7321661c1d91bf77fa17802452a51e1c4d270a72) |
| Raden Rafly Hanggaraksa Budiarto — Bandung Institute of Technology | Chameleon Cloud artifact implementing CLOCK with Adaptive Replacement, then upstreamed it as the merged CAR implementation | "Implemented the CAR (Clock with Adaptive Replacement) cache implementation on LibCacheSim"; he validated it against an independent Go implementation: "This implementation is being compared with [stfnmllr's GoCar]… My modification can be found here" | [Trovi](https://chameleoncloud.org/experiment/share/bac62a10-3868-4a77-9075-7e9247dd199b), [PR #131](https://github.com/1a1a11a/libCacheSim/pull/131) |
| Zirui Wang — PhD student, University of Virginia | Ran modern traces through it and asked for 64-bit object sizes | "I'm using this excellent repo, and many modern traces have large obj sizes. Would it be possible to support the uint_64 type of obj size in the next version?" | [issue #69](https://github.com/1a1a11a/libCacheSim/issues/69) |
| **system-intelligence-benchmark** — suite scoring LLM-designed systems heuristics; its `cache_algo_bench` task scores candidate eviction policies | Shells out to the built binary from `benchmarks/cache_algo_bench/src/cache_simulator/utils.py` | `command = f"""{LIBCACHSIM_PATH}/_build/bin/cachesim {cache_trace} oracleGeneral {cache_alg} {cache_cap} --ignore-obj-size 1 …"""` | [sys-intelligence/system-intelligence-benchmark](https://github.com/sys-intelligence/system-intelligence-benchmark/tree/46596edd8113a3eaf5646e49a42cc2a9fae3de4d) |

Budiarto's and Caridad's artifacts were found by scanning all 460 public Trovi artifacts.
Three others there — CLOCK-Pro, AdaptSize, and an S3-FIFO reproduction — are by the
project's own co-authors and are excluded as first-party; these two have no established
link, which is the default when none can be shown.

### Contributed from outside the project

Algorithms and infrastructure given back upstream by people with no project affiliation.
**These are counted separately from the adoption total**, because contributing a feature is a
different claim from adopting the tool, and for some of these rows the contribution is the
only evidence there is.

| Who | Contribution | Evidence | Source |
|---|---|---|---|
| Zhelong Zhao (`zztaki`) — Huazhong University of Science and Technology | The entire prefetch module and its three algorithms — Mithril, OBL and PG — negotiated through the `handle_find`/`handle_evict` interface over five merged PRs. A third-party SIGMETRICS '26 paper now uses them as baselines ([§1](#1-third-party-research)) | "I will add the `Mithril` algorithm that was mentioned in [the previous issue](https://github.com/1a1a11a/PyMimircache/issues/17). … Based on the above, I will submit a pull request. 😀" [`prefetch/`](/libCacheSim/cache/prefetch) ships here | [PR #17](https://github.com/1a1a11a/libCacheSim/pull/17), [#57](https://github.com/1a1a11a/libCacheSim/pull/57), [#59](https://github.com/1a1a11a/libCacheSim/pull/59) |
| Nathaniel Filardo (`nwf-msr`) — Microsoft | Found and fixed a parameter-propagation bug in the Random policies by reading the eviction sources | "`ccache_params_copy` in `Random.c` is set but not used thereafter: … I suspect the `ccache_params` on line 51 wants to be `ccache_params_copy` instead?" | [issue #51](https://github.com/1a1a11a/libCacheSim/issues/51), [PR #52](https://github.com/1a1a11a/libCacheSim/pull/52) |
| Liu Yang (`YangLiuWillow`) — Yale University | The first Rust bindings, working through `bindgen`'s `static inline` limitation and filing context upstream with rust-bindgen | "I created a new directory `libcachesim-rs` … With these, I write a Rust equivalent `test.c` file in `main.rs`" Closed unmerged; the design preceded the official bindings | [issue #125](https://github.com/1a1a11a/libCacheSim/issues/125#issuecomment-2746566305), [YangLiuWillow/libCacheSim](https://github.com/YangLiuWillow/libCacheSim/tree/a824c93630b7780a4f087460fd6d3a4d03b61de2) |
| Mack Wang (`mack-w`) | Took on removing the GLib dependency: audited every call site, chose a header-only hash map, and migrated the reader and MRC code | "forked repo: [mack-w/libCacheSim](https://github.com/mack-w/libCacheSim) … I checked compiled binaries and found that the majority of references to gLib are hash table functions." His fork carries `include/libCacheSim/hashmap.h`, a path that no commit on upstream `develop` has ever touched | [issue #133](https://github.com/1a1a11a/libCacheSim/issues/133#issuecomment-2879434943), [mack-w/libCacheSim](https://github.com/mack-w/libCacheSim/tree/6cecdd854fecead337638c6b513f353c57222d5e) |
| Mohammad Elsharqawy | An implementation of MultiQueue (ATC '01) with a validation table and documented deviations from the paper | "MQ with `n-queue=1` reproduces LRU exactly, as the paper predicts. Both give miss ratio 0.8299 and byte miss ratio 0.9730 on cloudPhysicsIO at 32 MiB." Open at the census date | [PR #318](https://github.com/1a1a11a/libCacheSim/pull/318) |

Smaller merged fixes came from outside too — a WTinyLFU over-eviction bug found by one
reporter and fixed by two others, a CodeQL workflow, `clang-tidy` cleanups, and a macOS
build repair nine months before the thesis work above. Those are contributor-funnel
activity rather than evidence of independent use, so they are not listed.

---

## 4. Distribution

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

**No third-party packaging exists.** An exact-name lookup across the 100 registries indexed
by [ecosyste.ms](https://packages.ecosyste.ms/api/v1/packages/lookup?name=libcachesim) —
Debian, Ubuntu, Alpine, nixpkgs, Guix, Homebrew, spack, vcpkg, conan, conda-forge, AUR,
crates.io and the rest — returns exactly one row, the project's own PyPI package. Reverse
dependencies are zero everywhere checked:
[deps.dev](https://api.deps.dev/v3alpha/systems/pypi/packages/libcachesim/versions/0.3.3:dependents)
reports `{"dependentCount":0,...}` for both the PyPI and npm packages, and
`dependent_packages_count` is 0 at ecosyste.ms. Three near-misses are worth naming so they
are not mistaken for ports: `proxy.golang.org`
[serves five versions](https://proxy.golang.org/github.com/1a1a11a/libcachesim/@v/list) of a
Go module synthesised from the git tags, which nothing imports (`?tab=importedby` is a 404)
and which cannot work, since the repository has no `go.mod`; the only other npm package,
`@realtmxi/libcachesim-node`, is a project collaborator's prototype published three days
before the in-tree package; and a Codeberg copy carrying four commits of Sphinx/Doxygen work
is the only non-GitHub forge copy found. The one true third-party redistribution is outside
package management: the T3-LRU fork in [§1](#1-third-party-research), archived with a DOI on
Zenodo. Software Heritage independently archives upstream, both package origins, and 34 fork
copies — five of which no longer resolve on GitHub, making the archive their only public
copy.

---

## 5. Related but not adoption

**Borrowed implementations and trace formats.** These take something from libCacheSim
without running or building on it, and count toward no total above — recorded because
`oracleGeneral` is becoming a de-facto interchange format for cache traces.

| Project | What it takes | Evidence | Source |
|---|---|---|---|
| **Cache is King: Smart Page Eviction with eBPF** — Zussman et al. (Columbia; IBM Research) | The LHD implementation, ported to eBPF | "We implement LHD using cachebpf, based on the implementation in libcachesim [69, 70, 72]." The paper shows no run of the tool; the group's SOSP '25 artifact keeps one tuning constant tied to it — "Inverse of value in libcachesim" — and its first author's own use is a fork ([§2](#2-third-party-forks)) | [arXiv:2502.02750](https://arxiv.org/abs/2502.02750), [cache-ext/cache_ext](https://github.com/cache-ext/cache_ext/tree/c50236cc98636ac7eaad5c19bc802d6e99a10540) |
| **Pelikan** `cachesim` (Rust) | The binary trace formats | "cachesim can import libCacheSim's binary trace formats"; "The `op` column uses the same integer encoding as libCacheSim's `req_op_e`". Published on crates.io as `cachesim-rs`, described as "Cache trace simulator for cache-rs crates, with libCacheSim-compatible trace formats" | [pelikan-io/cachesim](https://github.com/pelikan-io/cachesim/tree/9997462b2b3746fbd826837702027afbdea57d7a), [crates.io](https://crates.io/api/v1/crates/cachesim-rs) |
| **Caffeine** (Java) | The trace formats | Three parser families under `parser/libcachesim/`, with the attribution "libCacheSim</a> and distributed by the" cacheMon dataset project, registering `LCS_TRACE`, `LCS_ORACLE_GENERAL`, and `LCS_TWITTER` | [ben-manes/caffeine](https://github.com/ben-manes/caffeine/tree/9da6581ee366aa63c51e0dc96692d02f9c29ccff) |
| **Otter** (Go) | The trace formats | A `libcachesim` parser package: `OracleGeneralFormat = "oracleGeneral"`, `LibcachesimCSVFormat = "libcachesimCSV"` | [maypok86/otter](https://github.com/maypok86/otter/tree/8c526307556486ea0337280a4211135720bc29cc) |
| **Theine** (Python) | The trace format | A hand-rolled 24-byte `oracleGeneral` decoder over the project's own traces: `path = "benchmarks/trace/cluster52.oracleGeneral.sample10.zst"`. Weaker than the rows above: the repository never spells libCacheSim, so the only tie is the format and the trace filenames | [Yiling-J/theine](https://github.com/Yiling-J/theine/tree/7fbc169ed02418ca3f7680be611e8f0c79d99465) |
| **go-sieve** (Go) | The trace format and datasets | "We use traces from the [CacheLib / libCacheSim](https://cachelib.org/) trace" — its own mmap-based `oracleGeneral` reader, fed from the public trace bucket | [opencoff/go-sieve](https://github.com/opencoff/go-sieve/tree/43570259888f1fb1bb1765271c81ad26ba249bc4) |
| **s3-fifo** (Rust) — Dirkjan Ochtman | libCacheSim's S3-FIFO as the normative implementation | Its README points at a specific commit and line: "C implementation: https://github.com/1a1a11a/libCacheSim/blob/5fa68ef6902350aa9734398862bec7912d59f5e3/libCacheSim/cache/eviction/S3FIFO.c#L320" | [djc/s3-fifo](https://github.com/djc/s3-fifo/tree/30e5ac7290eac1cfb9a87bef72123cc0f874f420) |

**Downstream algorithm adoption.** SIEVE and S3-FIFO are reimplemented in third-party
systems. **Those systems do not use libCacheSim** — conflating the two is the most likely
way this census gets misread.

| System | Evidence | Source |
|---|---|---|
| Ceph | `src/common/web_cache.h`: "The implementation is based on SIEVE [0] with additional TTL", citing the NSDI '24 paper | [ceph/ceph](https://github.com/ceph/ceph/blob/5995d21863b3992bd9f463b5a0f774869351be36/src/common/web_cache.h) |
| Apache Traffic Server | `RamCacheS3FIFO.cc` implements the algorithm from the paper and the project website, "mirroring the libCacheSim reference (S3FIFO.c)"; its `NOTICE` names the artifact repository as the reference implementation | [apache/trafficserver](https://github.com/apache/trafficserver/blob/66d890830d36ac782498140fb2b2b71b5cadebbd/src/iocore/cache/RamCacheS3FIFO.cc) |
| TiDB | `pkg/infoschema/sieve.go` implements SIEVE — the file is named for the algorithm and its cache entry carries the algorithm's reference bit, "visited bool" | [pingcap/tidb](https://github.com/pingcap/tidb/blob/d5f9ca5690c0a53cac36002f9d2d2bdcba25f4fc/pkg/infoschema/sieve.go) |

A public code search matched the same pattern in a dozen more large projects — among them
Android's `androidx`, InfluxDB, Cloudflare's Pingora, Apache Pulsar and Jackrabbit Oak, and
DragonflyDB — each citing the SIEVE paper or the project website in a reimplementation, none
using the library. Those were not individually verified here and are not counted. A broader,
partly self-reported list is maintained on the
[SIEVE project site](https://cachemon.github.io/SIEVE-website/).

---

## 6. Repository signals

From the GitHub API on 2026-08-13. These measure attention, not deployment.

| Signal | Value |
|---|---|
| Stars | 340 |
| Forks | 111 |
| Contributors | 36 |
| Open issues (excluding pull requests) | 21 |
| Open pull requests | 9 |
| Created | 2020-06-19 |
| License | GPL-3.0 |

The issue and PR counts are split because the API's `open_issues_count` field — 30 here —
sums both, and reading it as an issue count overstates the backlog.

**Forks, measured rather than assumed.** `forks_count` is 111; the API lists 109, the other
two being forks of a fork. All 111 were resolved with `git ls-remote`, and every branch tip
was checked for commits reachable from none of upstream's 30 branches and 244 pull-request
refs. **Fifty-three carry no such commit** — plain mirrors. The other fifty-eight:

| Fork group | Count |
|---|---|
| Substantive independent third-party work — the ten forks in [§2](#2-third-party-forks) | 10 |
| Smaller but genuine third-party work — a WATT policy, sampled LRU and SIEVE variants, a Rust MRC tool, Docker packaging, a pybind11 wrapper, a 3L-Cache ablation, a standalone driver, 2021 work on the `exec` driver | 8 |
| Tied to an entry counted elsewhere in this census, through its paper or its author | 8 |
| Project members, co-authors, and alternate accounts | 6 |
| Student projects from the maintainer's course | 14 |
| A novel tip but no novel work: stale copies of upstream branches, editor and config edits, one machine-generated Rust port, and one 0-byte "algorithm" file | 12 |

Counting pull-request refs as upstream is what makes that table honest in the other
direction: work offered upstream as a PR is reachable from `refs/pull/*/head`, so the GLib
removal in [§3](#contributed-from-outside-the-project) scores zero novel commits here even
though it is absent from `develop` — a mirror by this measure, real work by any other.

A fork count is therefore not a user count, and the multiple depends entirely on where the
threshold sits: **26 of 111** carry independent third-party work, roughly a quarter, and
**10 of 111** carry substantive research work, an order of magnitude fewer. The earlier
editions of this census said of forks that "almost all are dormant copies of upstream text"; that
was wrong, and wrong in the direction that flatters nobody — it hid the forks that matter.

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
  number      = {census v1.2.0},
  year        = {2026},
  month       = aug,
  % replace <commit> with the permalink of the edition you read
  url         = {https://github.com/1a1a11a/libCacheSim/blob/<commit>/doc/adoption.md},
  note        = {Census date 2026-08-13}
}
```

`dc80ebf` in the header is the repository state the census describes, not a permalink for
this file — that commit does not contain it. To cite libCacheSim itself, use
[`references.md`](/references.md).

**To add an entry:** open a PR with the source URL, the verbatim sentence located in raw
source, and the date you verified it. Bump the version and append to the changelog.

---

## Changelog

| Version | Date | Change |
|---|---|---|
| 1.2.0 | 2026-08-13 | Added 20 entries from a GitHub sweep: all 111 forks triaged against upstream's object graph, ~80 public code-search queries, the repository's own 318 issues and PRs and 7 discussions, dependency and archive graphs, the citation graphs of four papers, and the Chameleon Trovi catalogue. New: two journal papers (SIGMETRICS '26 and IEEE TMC), a third-party forks section with 11 rows including MongoDB's WiredTiger emulation, the SOSP '23 artifact-evaluation committee, 5 outside contributors, 4 trace-format borrowers, and Apache Traffic Server. Corrected the claim that forks are almost all dormant: 58 of 111 carry a commit reachable from no upstream ref, and the 58 are partitioned in §6. 34 third-party adoption entries — 15 research, 11 forks, 8 practitioner and community — plus 5 outside contributions, 4 distribution channels, 10 borrowed-implementation and downstream rows, and repository signals, each counted separately. |
| 1.1.0 | 2026-08-13 | Removed first-party entries: the section of the project's own papers and artifacts, two Chameleon artifacts by project co-authors, and the CacheBench and cache_dataset ecosystem rows. Every remaining entry is third-party. 19 entries — 13 third-party research, 6 practitioner, community, and ecosystem — plus 4 distribution channels. Repository signals refreshed. |
| 1.0.0 | 2026-08-13 | First edition. 31 entries — 8 first-party papers and artifacts, 13 third-party works, 7 practitioner and community entries, 3 ecosystem projects — of which 19 are third-party, plus 4 distribution channels. Separately recorded and not counted: the OSDI '20 paper that introduced the simulator, 4 borrowed-implementation and trace-format rows, 2 downstream algorithm adopters, and repository signals. |
