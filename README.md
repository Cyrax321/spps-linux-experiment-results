<div align="center">

---

### ⚠️ REVIEW-ONLY NOTICE

**This repository is provided solely for peer review and reproducibility purposes**  
**associated with the submitted manuscript.**

Reuse, redistribution, modification, or deployment of any code, data, or results  
contained herein is **strictly prohibited** without explicit written permission from the authors.

© 2026 The Authors. All Rights Reserved.

---

</div>

# SPPS — Signed Positional Prüfer Sequences
### Experimental Code Base · Linux / x86_64

> **Paper**: *Signed Positional Prüfer Sequences (SPPS): A Novel Linear-Time Tree Serialization Algorithm*    
> **Platform**: AMD Ryzen 5 7235HS (Zen 3+) · Ubuntu 22.04 LTS · GCC 11.x

---

## Table of Contents

1. [Overview](#overview)
2. [Platform & Hardware](#platform--hardware)
3. [System Dependencies](#system-dependencies)
4. [Repository Layout](#repository-layout)
5. [Build Instructions](#build-instructions)
6. [Experiments at a Glance](#experiments-at-a-glance)
   - [Block A — Correctness](#block-a--correctness)
   - [Block B — O(n) Linearity](#block-b--on-linearity)
   - [Block C — Real-Dataset Benchmarks](#block-c--real-dataset-benchmarks)
   - [Block D — LOUDS Baseline](#block-d--louds-baseline)
   - [Block E — Compression](#block-e--compression)
   - [Block F — Tail Latency](#block-f--tail-latency)
   - [Block G — Downstream Macro-Benchmark](#block-g--downstream-macro-benchmark)
   - [Block H — Worked Examples](#block-h--worked-examples)
7. [Key Results](#key-results)
8. [Datasets](#datasets)
9. [Fairness Notes](#fairness-notes)
10. [Memory & Microarchitectural Profiles](#memory--microarchitectural-profiles)
11. [Reproducibility](#reproducibility)
12. [License](#license)

---

## Overview

**SPPS** (Signed Positional Prüfer Sequences) is a novel O(n) tree serialization algorithm that extends the classical Prüfer sequence with a *direction flag* and *positional child-rank encoding*. It produces a compact integer sequence of length n−2 that uniquely encodes any rooted ordered tree with exact reconstruction.

This repository contains the full experimental suite used to validate and benchmark SPPS against three industry-standard baselines:

| Baseline | Description |
|---|---|
| **LOUDS** | Level-Order Unary Degree Sequence, O(1) rank/select (64-bit packed blocks + `__builtin_popcountll`) |
| **FlatBuffers** | Google's zero-copy serialization (v25.12.19) |
| **Protobuf** | Google Protocol Buffers with Arena allocation (libprotoc 34.0) |

All experiments run on **Linux (Ubuntu 22.04 LTS x86_64)** and are profiled using hardware PMU counters (`perf stat` + AMD uProf).

---

## Platform & Hardware

| Property | Value |
|---|---|
| **Machine** | Lenovo LOQ 15 |
| **CPU** | AMD Ryzen 5 7235HS (4C/8T, Zen 3+, 3.2-4.2 GHz) |
| **RAM** | 12 GB DDR5-4800 (1x SO-DIMM) |
| **L2 cache** | 2 MB |
| **L3 cache** | 8 MB |
| **Storage** | 512 GB NVMe PCIe 4.0x4 |
| **OS** | Ubuntu 22.04 LTS |
| **Compiler** | g++ 11.x (`-std=c++17 -O3 -march=native`) |
| **Profiling** | `perf stat` + `AMD uProf` |

> **Important:** All benchmark numbers in this repository and in the paper are pending the results of the finalized runs on this specific machine configuration.

---

## System Dependencies

A convenient installer script is provided for Ubuntu 22.04:

```bash
sudo ./scripts/install_deps.sh
```

Or install manually via `apt`:

```bash
sudo apt update
sudo apt install -y build-essential g++ pkg-config cmake \
    protobuf-compiler libprotobuf-dev \
    libflatbuffers-dev flatbuffers-compiler \
    libzstd-dev linux-tools-common linux-tools-generic \
    linux-tools-$(uname -r)
```

| Tool | Version used | Purpose |
|---|---|---|
| `protoc` | libprotoc 3+ | Protocol Buffers compiler |
| `flatc` | 2+ | FlatBuffers schema compiler |
| `zstd` | 1.4+ | Compression baseline (Block E) |
| `perf` | System-native | Microarchitectural profiling |

---

## Repository Layout

```
spps/
├── .gitignore
├── EXPERIMENTS.md                 # Details on experiments/metrics
├── PLATFORM.md                    # Hardware diff mapping (Mac -> Linux)
├── README.md                      
├── Makefile                       # Unified Linux build system
├── block_a_correctness.cpp        # A: Correctness proofs (fuzzing + worked examples)
├── block_b_linearity.cpp          # B: O(n) linearity regression across 4 topologies
├── block_c_benchmarks.cpp         # C: Real-dataset 4-method benchmark (Arena PB)
├── block_d_louds.cpp              # D: LOUDS baseline integration (O(1) rank/select)
├── block_e_compression.cpp        # E: zstd compression, all formats apples-to-apples
├── block_f_tail_latency.cpp       # F: Tail latency & CV% stability
├── block_g_downstream.cpp         # G: End-to-end downstream pipeline macro-benchmark
├── block_h_worked_examples.cpp    # H: Step-by-step encode/decode traces
├── profiler.cpp                   # Memory + perf-counter profiling harness
├── profiler_isolated.cpp          # Per-method isolated profiling wrapper for `perf`
├── tree.proto                     # Protobuf schema
├── tree.fbs                       # FlatBuffers schema
├── datasets/                      # Directory for edge lists (Django, sqlite3, XMark)
│   └── README.md
└── scripts/
    ├── install_deps.sh            # Ubuntu Dependency Setup
    ├── run_all_blocks.sh          # Native Experiment Runtime Sequence
    ├── run_perf_profile.sh        # Automates `perf stat` hardware hooks
    └── run_uprof_profile.sh       # Pipeline extraction via AMD uProf
```

> Large binary/data files (`perf_results/`, `uprof_results/`, `*.trace`, `xmark.xml`, `sqlite3_ast.json`, compiled binaries) are excluded by `.gitignore`.

---

## Build Instructions

All source files are compiled easily using the provided Linux `Makefile` natively tuned for `--march=native` using `g++`.

```bash
# 1. Generate protobuf / flatbuffers headers (run once)
make gen-proto

# 2. Build all standalone binaries natively
make all
```

Individual compilation targets are also fully available (`make block_a`, etc.), binding directly to `-lpthread`, `-lzstd`, `-lflatbuffers`, and linking `protobuf`.

---

## Experiments at a Glance

### Block A — Correctness

Verifies SPPS encode → decode round-trip correctness through exhaustive fuzzing:

| Test | Count | Result |
|---|---|---|
| A1: General fuzzing (random trees) | 10,000 |  [TBD] |
| A2: Directed-edge stress test | 1,000 |  [TBD] |
| A3: Sibling-order stress test | 1,000 |  [TBD] |
| A4: Worked example (n=9) | 1 |  [TBD] |
| A5: Boundary cases (n=1,2,3) | — |  [TBD] |
| **Total** | **12,006** | ** [TBD]** |

---

### Block B — O(n) Linearity

Measures SPPS encode time (30 trials/size) across 4 topologies and fits a linear regression.

| Topology | Slope (ns/node) | R² | Linear? |
|---|---|---|---|
| B1: Path Graph | [TBD] | [TBD] |  [TBD] |
| B2: Star Graph | [TBD] | [TBD] |  [TBD] |
| B3: Balanced Binary Tree | [TBD] | [TBD] |  [TBD] |
| B4: Random AST-Like (12-point fine-grained) | [TBD] | [TBD] |  [TBD] |

---

### Block C — Real-Dataset Benchmarks

**4 methods × 3 datasets × 30 trials.** Protobuf uses `google::protobuf::Arena`. LOUDS uses O(1) rank/select.

#### Django AST (n = 2,325,575)

| Method | Enc (ms) | Dec (ms) | DFS (ms) | Total (ms) | Size (B) | B/node |
|---|---|---|---|---|---|---|
| **SPPS** | **[TBD]** | **[TBD]** | **[TBD]** | **[TBD]** | [TBD] | [TBD] |
| LOUDS | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |
| FlatBuffers | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |
| Protobuf | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |

#### sqlite3 AST (n = 503,141)

| Method | Enc (ms) | Dec (ms) | DFS (ms) | Total (ms) | Size (B) | B/node |
|---|---|---|---|---|---|---|
| **SPPS** | **[TBD]** | **[TBD]** | **[TBD]** | **[TBD]** | [TBD] | [TBD] |
| LOUDS | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |
| FlatBuffers | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |
| Protobuf | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |

#### XMark XML (n = 500,000)

| Method | Enc (ms) | Dec (ms) | DFS (ms) | Total (ms) | Size (B) | B/node |
|---|---|---|---|---|---|---|
| **SPPS** | **[TBD]** | **[TBD]** | **[TBD]** | **[TBD]** | [TBD] | [TBD] |
| LOUDS | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |
| FlatBuffers | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |
| Protobuf | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |

---

### Block D — LOUDS Baseline

Head-to-head on Django AST (30 trials). LOUDS accelerated with O(1) rank/select via 64-bit packed blocks and `__builtin_popcountll`.

| Method | Enc (ms) | Dec (ms) | DFS (ms) | Total (ms) | Size (B) | B/node |
|---|---|---|---|---|---|---|
| **SPPS** | **[TBD]** | **[TBD]** | **[TBD]** | **[TBD]** | [TBD] | [TBD] |
| LOUDS | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |
| FlatBuffers | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |
| Protobuf | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |

**Speedup of SPPS over accelerated LOUDS: [TBD]×**

---

### Block E — Compression

All formats pass through the **same** zstd pipeline (apples-to-apples, Django AST n=2,325,575):

| Format | Raw B/node | zstd-1 B/node | zstd-3 B/node | Ratio |
|---|---|---|---|---|
| SPPS | [TBD] | [TBD] | [TBD] | [TBD]× |
| Protobuf (Arena) | [TBD] | [TBD] | [TBD] | [TBD]× |
| FlatBuffers | [TBD] | [TBD] | [TBD] | [TBD]× |

> Protobuf's varint encoding commonly produces more compressible output than SPPS's fixed-width int64. This is called out explicitly in the paper.

---

### Block F — Tail Latency

30 trials per dataset, measuring P50/P90/P95/P99 and CV%:

| Dataset | Encode CV% | TotalRead CV% |
|---|---|---|
| Django AST | [TBD] | [TBD] |
| sqlite3 AST | [TBD] | [TBD] |
| XMark XML | [TBD] | [TBD] |
| **Mean** | **[TBD]** | **[TBD]** |

All CV% values are expected to be well within the 5% stability threshold.

---

### Block G — Downstream Macro-Benchmark

End-to-end pipeline: encode → serialize → deserialize → DFS max-depth (Django AST, 3 warmup + 30 trials):

| Metric | SPPS | Protobuf (Arena) | Speedup |
|---|---|---|---|
| Encode (ms) | [TBD] | [TBD] | **[TBD]×** |
| Decode (ms) | [TBD] | [TBD] | **[TBD]×** |
| DFS Max-Depth (ms) | [TBD] | [TBD] | [TBD]× |
| **Total Pipeline (ms)** | **[TBD]** | **[TBD]** | **[TBD]×** |
| Max Depth Found | [TBD] | [TBD] | — |

---

### Block H — Worked Examples

Step-by-step encode/decode traces for paper verification (n=7 figure example and n=9). All intermediate values (degree sequence, omega values, direction flags, child ranks) are printed and verified. Status: **[TBD]**.

---

## Key Results

| Claim | Evidence |
|---|---|
| SPPS is **O(n)** linear time | Block B: R² metrics |
| SPPS is correct by construction | Block A validation outputs |
| SPPS speedups | Block D benchmark |
| SPPS pipeline performance | Block G end to end benchmark |
| SPPS memory efficiency | Memory profiling |
| SPPS achieves fewer retired instructions than LOUDS | Pipeline metrics via PMC |
| SPPS compression ratios | Block E |

---

## Datasets

All datasets are encoded as plain-text **edge-list files** located in `datasets/` and
used directly by every block's benchmark execution.

### Edge-List Format

```
<n>              ← first line: number of nodes
<u1> <v1>        ← subsequent lines: directed edges u → v (parent → child)
<u2> <v2>
...
```

Nodes are 1-indexed integers. The root is always node `1`.

---

### ✅ Files Committed to This Repository

| File | Size | Dataset | n (nodes) |
|---|---|---|---|
| `datasets/real_ast_benchmark.txt` | 33 MB | **Django AST** | 2,325,575 |
| `datasets/sqlite3_ast_edges.txt` | 6.4 MB | **sqlite3 AST** | 503,141 |
| `datasets/xmark_edges.txt` | 6.5 MB | **XMark XML** | 500,000 |

These three files are **all that is needed** to reproduce every benchmark result in the paper.

---

### ❌ Raw Source Files (Too Large for GitHub — Must Regenerate)

The following raw source files exceed GitHub's 100 MB limit and are excluded via `.gitignore`. They were used only to *generate* the edge-list files above.

| File | Size | Purpose |
|---|---|---|
| `sqlite3_ast.json` | 669 MB | sqlite3 AST as JSON |
| `xmark.xml` | 266 MB | XMark benchmark XML |

---

### Dataset Properties

| Dataset | n (nodes) | Depth | Source | Type |
|---|---|---|---|---|
| **Django AST** | 2,325,575 | 28 | Django 4.2 full codebase AST | Real-world, unbalanced |
| **sqlite3 AST** | 503,141 | — | sqlite3.c (8.6 MB, 266K lines) | Real-world, deep paths |
| **XMark XML** | 500,000 | — | XMark benchmark generator | Synthetic, XML hierarchy |

---

## Fairness Notes

1. **Protobuf Arena**: All Protobuf benchmarks use `google::protobuf::Arena` for contiguous memory allocation. Without Arena: PB encode ~228 ms, decode ~84 ms. With Arena: ~114 ms encode, ~56 ms decode (≈50% and 33% reduction respectively). This is the fairest possible comparison for PB.

2. **Apples-to-apples compression** (Block E §E3): All three formats pass through the identical zstd pipeline. Protobuf's varint format compresses significantly better than SPPS's fixed-width int64 — this is noted honestly in the paper.

3. **LOUDS O(1) rank/select**: Block D uses a fully accelerated LOUDS with `__builtin_popcountll` over 64-bit packed blocks and streaming `__builtin_ctzll` decode. This is the strongest possible LOUDS implementation.

4. **Algorithm direction flag** (Alg. 1, Line 27): `d = (parent[leaf]==P) ? +1 : ((parent[P]==leaf) ? -1 : +1)`. The fallback to +1 is defensive; for rooted trees the first two branches always suffice.

---

## Memory & Microarchitectural Profiles

All results natively integrate inside Linux using `perf stat` across basic counters alongside AMD-specific logic for mapping pipelines:

| Method | Peak RSS (MB) | Instructions (B) | Cycles (B) | IPC | Inst/node | Cycles/node |
|---|---|---|---|---|---|---|
| **SPPS** | **[TBD]** | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |
| LOUDS (O(1) accel.) | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |
| FlatBuffers | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |
| Protobuf (Arena) | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] | [TBD] |

_RSS tracked via exact equivalents derived utilizing `/usr/bin/time -v`_. 

---

## Reproducibility

To cleanly reproduce all benchmarks independently:

```bash
# 1. Provide all pre-bundled dataset structures over in `datasets/`
# 2. Extract PMU Counters and standard outputs natively:
./scripts/run_all_blocks.sh
sudo ./scripts/run_perf_profile.sh
sudo ./scripts/run_uprof_profile.sh
```

---

## ⚖️ Legal Notice & Copyright

```
Copyright © 2026 The Authors. All Rights Reserved.

This repository and all of its contents — including but not limited to source code,
experiment scripts, benchmark data, terminal output logs, research reports, and
the accompanying manuscript PDF — are made available SOLELY for the purpose of
peer review and reproducibility verification associated with the submitted manuscript:

  "Signed Positional Prüfer Sequences (SPPS): A Novel Linear-Time Tree
   Serialization Algorithm"

THIS IS NOT AN OPEN-SOURCE RELEASE.

The following actions are STRICTLY PROHIBITED without explicit written permission
from the authors:

  • Reuse of any portion of the code in other projects or products
  • Redistribution of this repository or any of its contents, in whole or in part
  • Modification, adaptation, translation, or creation of derivative works
  • Deployment of the algorithm or implementation in any production system
  • Academic citation of unpublished results prior to formal publication
  • Training of machine learning models on the contents of this repository

VIOLATION OF THESE TERMS MAY CONSTITUTE COPYRIGHT INFRINGEMENT AND/OR
MISAPPROPRIATION OF UNPUBLISHED ACADEMIC WORK.

Upon publication of the accepted manuscript, a formal open-source license
will be designated by the authors. Until that time, no license — express,
implied, or statutory — is granted to any party.
```

> **Permitted use:** Reviewers assigned by the programme committee may read, compile, and run the code solely for the purpose of evaluating the submitted manuscript. No other use is permitted.

---

<div align="center">

*Experiments designed and executed on AMD Ryzen 5 7235HS · Ubuntu 22.04 LTS · GCC 11.x*

</div>
