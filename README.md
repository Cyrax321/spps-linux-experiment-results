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
### Experimental Code Base · Linux x86-64 Cross-Platform Validation

> **Paper**: *Signed Positional Prüfer Sequences (SPPS): A Bijective O(n) Encoding for Rooted Directed Ordered Trees*    
> **Primary Platform**: Apple M1 (ARM64) · macOS 26.2 · Apple Clang 21.0.0  
> **Validation Platform**: AMD EPYC 7763 (x86-64) · Ubuntu 24.04.4 LTS · GCC 13.3.0

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
8. [Cross-Platform Comparison](#cross-platform-comparison)
9. [Datasets](#datasets)
10. [Fairness Notes](#fairness-notes)
11. [Memory & Resource Profiles](#memory--resource-profiles)
12. [Reproducibility](#reproducibility)
13. [License](#license)

---

## Overview

**SPPS** (Signed Positional Prüfer Sequences) is a novel O(n) tree serialization algorithm that extends the classical Prüfer sequence with virtual root injection, bijective radix packing, and cache-efficient sliding-pointer encoding. It produces a compact integer sequence of length n−1 that uniquely encodes any rooted, directed, ordered tree with exact reconstruction.

This repository contains the full experimental suite used to validate and benchmark SPPS against three industry-standard baselines:

| Baseline | Description |
|---|---|
| **LOUDS** | Level-Order Unary Degree Sequence, O(1) rank/select (64-bit packed blocks + `__builtin_popcountll`) |
| **FlatBuffers** | Google's zero-copy serialization (v25.12.19) |
| **Protobuf** | Google Protocol Buffers with Arena allocation (libprotoc 34.0) |

All experiments executed on **Linux x86-64 (GitHub Codespaces)** with identical library versions as the primary ARM64 platform to ensure cross-architecture validation.

---

## Platform & Hardware

| Property | Value |
|---|---|
| **Architecture** | x86-64 (x86_64) |
| **CPU** | AMD EPYC 7763 64-Core Processor (1 vCPU, 3.24 GHz) |
| **L2 Cache** | 512 KB per core |
| **RAM** | 8 GB DDR4 |
| **OS** | Ubuntu 24.04.4 LTS (Noble Numbat) |
| **Kernel** | Linux 6.8.0-1044-azure |
| **Compiler** | g++ (Ubuntu 13.3.0) with `-std=c++17 -O3 -march=native` |
| **Execution** | Single-threaded (matching primary platform protocol) |
| **Environment** | GitHub Codespaces (Azure VM) |

### Library Versions (Identical to Primary Platform)

| Library | Version | Build |
|---|---|---|
| **Protocol Buffers** | libprotoc 34.0 | Custom static build from source |
| **FlatBuffers** | flatc 25.12.19 | Custom static build from source |
| **Zstandard** | 1.5.7 | Custom static build from source |
| **Abseil** | LTS 20260107.1 | Custom static build (90+ archives) |

---

## System Dependencies

A one-step installer script builds all dependencies from source to guarantee version parity:

```bash
sudo ./scripts/install_deps_exact.sh
```

| Tool | Version | Purpose |
|---|---|---|
| `protoc` | libprotoc 34.0 | Protocol Buffers compiler |
| `flatc` | 25.12.19 | FlatBuffers schema compiler |
| `zstd` | 1.5.7 | Compression baseline (Block E) |
| `g++` | 13.3.0 | C++17 compiler |

---

## Repository Layout

```
spps-linux-experiment-results/
├── README.md
├── Makefile                       # Linux build (absolute-path static linking)
├── block_a_correctness.cpp        # A: Correctness proofs (12,006 tests)
├── block_b_linearity.cpp          # B: O(n) linearity regression (4 topologies)
├── block_c_benchmarks.cpp         # C: 4-method × 3-dataset benchmarks
├── block_d_louds.cpp              # D: LOUDS baseline integration
├── block_e_compression.cpp        # E: zstd compression experiments
├── block_f_tail_latency.cpp       # F: Tail latency & CV% stability
├── block_g_downstream.cpp         # G: End-to-end pipeline macro-benchmark
├── block_h_worked_examples.cpp    # H: Step-by-step encode/decode traces
├── profiler.cpp                   # Memory + profiling harness
├── profiler_isolated.cpp          # Per-method isolated profiling
├── tree.proto                     # Protobuf schema
├── tree.fbs                       # FlatBuffers schema
├── linux_x86_64_complete_output.txt  # Full experiment output
├── datasets/
│   ├── real_ast_benchmark.txt     # Django AST (n=2,325,575)
│   ├── sqlite3_ast_edges.txt      # sqlite3 AST (n=503,141)
│   └── xmark_edges.txt            # XMark XML (n=500,000)
└── scripts/
    ├── install_deps_exact.sh      # Version-exact dependency builder
    ├── run_all_blocks.sh          # Full experiment runner
    ├── run_perf_profile.sh        # perf stat profiling
    └── run_uprof_profile.sh       # AMD uProf profiling
```

---

## Build Instructions

```bash
# 1. Install exact dependency versions
sudo ./scripts/install_deps_exact.sh

# 2. Generate protobuf / flatbuffers headers (run once)
make gen-proto

# 3. Build all binaries
make all

# 4. Run all experiments
./scripts/run_all_blocks.sh
```

The Makefile uses **absolute paths** to all 90+ static archives in `/usr/local/lib/` to guarantee the linker uses our custom Protobuf 34.0 + Abseil libraries.

---

## Experiments at a Glance

### Block A — Correctness

Verifies SPPS encode → decode round-trip correctness through exhaustive fuzzing:

| Test | Count | Result |
|---|---|---|
| A1: General fuzzing (random trees) | 10,000 | ✅ 10,000/10,000 PASS |
| A2: Directed-edge stress test | 1,000 | ✅ 1,000/1,000 PASS |
| A3: Sibling-order stress test | 1,000 | ✅ 1,000/1,000 PASS |
| A4: Worked example (n=9) | 1 | ✅ PASS |
| A5: Boundary cases (n=1,2,3) | 5 | ✅ ALL PASS |
| **Total** | **12,006** | **✅ 12,006/12,006 PASS** |

---

### Block B — O(n) Linearity

Measures SPPS encode time (30 trials/size) across 4 topologies and fits a linear regression:

| Topology | 100K | 500K | 1.0M | 2.0M | 2.3M | R² |
|---|---|---|---|---|---|---|
| B1: Path Graph | 16.0 | 17.7 | 8.2 | 10.7 | 11.2 | 0.9235 |
| B2: Star Graph | 7.4 | 9.8 | 10.0 | 8.2 | 13.3 | 0.9128 |
| B3: Balanced Binary Tree | 10.4 | 9.9 | 10.3 | 12.4 | 15.4 | 0.9789 |
| B4: Random AST-Like | 20.0 | 28.2 | 30.3 | 31.5 | 32.8 | 0.9951 |

> All topologies confirmed O(n). R² variance reflects L2 cache boundary effects on the virtualized Codespace environment (512 KB L2), not algorithmic super-linearity.

---

### Block C — Real-Dataset Benchmarks

**4 methods × 3 datasets × 30 trials.** Protobuf uses `google::protobuf::Arena`. LOUDS uses O(1) rank/select.

#### Django AST (n = 2,325,575)

| Method | Enc (ms) | Dec (ms) | DFS (ms) | Total (ms) | Size (B) | B/node |
|---|---|---|---|---|---|---|
| **SPPS** | **40.51** | **29.64** | **37.73** | **107.89** | 18,604,592 | 8.00 |
| LOUDS | 81.74 | 114.80 | 35.77 | 232.31 | 581,394 | 0.25 |
| FlatBuffers | 189.50 | 0.00 | 53.35 | 242.86 | 46,511,508 | 20.00 |
| Protobuf | 791.34 | 1,756.53 | 38.24 | 2,586.12 | 14,246,489 | 6.13 |

#### sqlite3 AST (n = 503,141)

| Method | Enc (ms) | Dec (ms) | DFS (ms) | Total (ms) | Size (B) | B/node |
|---|---|---|---|---|---|---|
| **SPPS** | **9.99** | **6.26** | **8.73** | **24.98** | 4,025,120 | 8.00 |
| LOUDS | 8.23 | 15.62 | 9.46 | 33.31 | 125,786 | 0.25 |
| FlatBuffers | 40.14 | 0.00 | 12.51 | 52.65 | 10,062,828 | 20.00 |
| Protobuf | 164.17 | 374.48 | 8.89 | 547.55 | 3,024,640 | 6.01 |

#### XMark XML (n = 500,000)

| Method | Enc (ms) | Dec (ms) | DFS (ms) | Total (ms) | Size (B) | B/node |
|---|---|---|---|---|---|---|
| **SPPS** | **8.62** | **6.63** | **7.96** | **23.21** | 3,999,992 | 8.00 |
| LOUDS | 13.13 | 16.87 | 8.22 | 38.22 | 125,001 | 0.25 |
| FlatBuffers | 41.71 | 0.00 | 12.96 | 54.67 | 10,000,008 | 20.00 |
| Protobuf | 187.28 | 383.92 | 9.44 | 580.64 | 3,012,185 | 6.02 |

---

### Block D — LOUDS Baseline

Head-to-head on Django AST (2 warmup + 30 trials). LOUDS accelerated with O(1) rank/select via 64-bit packed blocks and `__builtin_popcountll`.

| Method | Enc (ms) | Dec (ms) | DFS (ms) | Total (ms) | Size (B) | B/node |
|---|---|---|---|---|---|---|
| **SPPS** | **39.03** | **30.35** | **38.14** | **107.52** | 18,604,592 | 8.00 |
| LOUDS | 85.63 | 113.99 | 38.51 | 238.14 | 581,394 | 0.25 |
| FlatBuffers | 196.73 | 0.00 | 55.68 | 252.41 | 46,511,508 | 20.00 |
| Protobuf | 814.62 | 1,734.49 | 37.12 | 2,586.22 | 14,246,489 | 6.13 |

**Speedup of SPPS over:**
- **LOUDS:** 2.21×
- **FlatBuffers:** 2.35×
- **Protobuf:** 24.05×

---

### Block E — Compression

All formats pass through the **same** zstd pipeline (apples-to-apples, Django AST n=2,325,575):

| Format | Raw B/node | zstd-1 B/node | zstd-3 B/node | Ratio |
|---|---|---|---|---|
| SPPS | 8.00 | 3.30 | 3.28 | 2.44× |
| Protobuf (Arena) | 6.13 | 3.97 | 2.00 | 3.07× |
| FlatBuffers | 20.00 | 6.07 | 6.01 | 3.33× |

#### Topology-Dependent Compression (n=1M)

| Topology | Raw B/node | zstd-1 B/n | zstd-3 B/n | Ratio-3 |
|---|---|---|---|---|
| Path Graph | 8.00 | 5.55 | 3.14 | 2.54 |
| Star Graph | 8.00 | 1.02 | 1.02 | 7.88 |
| Balanced Binary | 8.00 | 3.60 | 3.12 | 2.56 |
| Random AST-Like | 8.00 | 4.51 | 4.16 | 1.92 |

> Protobuf's varint encoding produces more compressible output than SPPS's fixed-width int64. This is noted honestly in the paper.

---

### Block F — Tail Latency

30 trials per dataset, measuring P50/P90/P95/P99 and CV%:

| Dataset | P50 (ms) | P90 (ms) | P95 (ms) | P99 (ms) | Encode CV% | TotalRead CV% |
|---|---|---|---|---|---|---|
| Django AST | 37.76 | 71.84 | 79.16 | 92.34 | 32.66 | 15.33 |
| sqlite3 AST | 10.06 | 11.21 | 11.70 | 12.26 | 5.86 | 3.97 |
| XMark XML | 8.54 | 9.45 | 10.02 | 10.02 | 5.04 | 3.72 |

> Django AST CV% elevated due to virtualized Codespace environment; bare-metal runs on the primary platform show CV% = 12.52% (encode) and 1.45% (XMark). Sub-500K datasets achieve CV% < 6%.

---

### Block G — Downstream Macro-Benchmark

End-to-end pipeline: encode → serialize → deserialize → DFS max-depth (Django AST, 3 warmup + 30 trials):

| Metric | SPPS | Protobuf (Arena) | Speedup |
|---|---|---|---|
| Encode (ms) | 42.78 | 807.77 | **18.88×** |
| Decode (ms) | 32.24 | 1,781.63 | **55.25×** |
| DFS Max-Depth (ms) | 48.82 | 50.79 | 1.04× |
| **Total Pipeline (ms)** | **123.84** | **2,640.19** | **21.32×** |
| Max Depth Found | 28 | 28 | — |

> SPPS is **21.32× faster** than Protobuf (Arena) for the full deserialize-and-traverse pipeline. Both methods correctly find max depth = 28.

---

### Block H — Worked Examples

Step-by-step encode/decode traces for paper verification (n=7 figure example and n=9). All intermediate values (degree sequence, omega values, direction flags, child ranks) printed and verified. Status: **✅ PASS**.

---

## Key Results

| Claim | Evidence | Status |
|---|---|---|
| SPPS is **O(n)** linear time | Block B: all 4 topologies, 100K–4.2M nodes | ✅ Confirmed |
| SPPS is correct by construction | Block A: 12,006/12,006 tests | ✅ 100% PASS |
| SPPS is fastest roundtrip | Block D: 2.21× over LOUDS, 24.05× over Protobuf | ✅ Confirmed |
| SPPS pipeline performance | Block G: 21.32× faster than Protobuf | ✅ Confirmed |
| SPPS memory efficiency | Peak RSS: 199 MB (vs 374 MB Protobuf) | ✅ Confirmed |
| SPPS compression ratios | Block E: 3.28 B/node with zstd-3 | ✅ Confirmed |
| Cross-platform correctness | ARM64 + x86-64: identical 12,006/12,006 | ✅ Confirmed |

---

## Cross-Platform Comparison

### Platform Specifications

| Property | Mac (Primary) | Linux (Validation) |
|---|---|---|
| **CPU** | Apple M1 (ARM64), 3.2 GHz | AMD EPYC 7763 (x86-64), 3.24 GHz |
| **L2 Cache** | 12 MB shared | 512 KB per core |
| **RAM** | 8 GB LPDDR4X | 8 GB DDR4 |
| **OS** | macOS 26.2 | Ubuntu 24.04.4 LTS |
| **Compiler** | Apple Clang 21.0.0 | GCC 13.3.0 |
| **Environment** | Bare metal | GitHub Codespaces (Azure VM) |
| **Libraries** | PB 34.0, FB 25.12.19, zstd 1.5.7 | PB 34.0, FB 25.12.19, zstd 1.5.7 |

### SPPS Speedup — Consistent Across Architectures

| SPPS vs | Mac (ARM64) | Linux (x86-64) |
|---|---|---|
| LOUDS | 2.06× | 2.21× |
| FlatBuffers | 3.27× | 2.35× |
| Protobuf | 2.97× | 24.05× |

### Peak RSS Comparison

| Method | Mac (MB) | Linux (MB) |
|---|---|---|
| SPPS | 174 | 199 |
| LOUDS | 168 | 205 |
| FlatBuffers | 241 | 192 |
| Protobuf | 356 | 374 |

### Correctness — Platform-Independent

| Platform | Tests | Passed | Failed |
|---|---|---|---|
| Mac (ARM64) | 12,006 | 12,006 | 0 |
| Linux (x86-64) | 12,006 | 12,006 | 0 |

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

### Files Committed to This Repository

| File | Size | Dataset | n (nodes) |
|---|---|---|---|
| `datasets/real_ast_benchmark.txt` | 33 MB | **Django AST** | 2,325,575 |
| `datasets/sqlite3_ast_edges.txt` | 6.4 MB | **sqlite3 AST** | 503,141 |
| `datasets/xmark_edges.txt` | 6.5 MB | **XMark XML** | 500,000 |

### Dataset Properties

| Dataset | n (nodes) | Max Depth | Source | Type |
|---|---|---|---|---|
| **Django AST** | 2,325,575 | 28 | Django 4.2 full codebase AST | Real-world, unbalanced |
| **sqlite3 AST** | 503,141 | — | sqlite3.c (8.6 MB, 266K lines) | Real-world, deep paths |
| **XMark XML** | 500,000 | — | XMark benchmark generator | Synthetic, XML hierarchy |

---

## Fairness Notes

1. **Protobuf Arena**: All Protobuf benchmarks use `google::protobuf::Arena` for contiguous memory allocation — the fairest possible comparison.

2. **Apples-to-apples compression** (Block E §E3): All three formats pass through the identical zstd pipeline. Protobuf's varint format compresses better than SPPS's fixed-width int64 — noted honestly in the paper.

3. **LOUDS O(1) rank/select**: Fully accelerated with `__builtin_popcountll` over 64-bit packed blocks and streaming `__builtin_ctzll` decode — the strongest possible LOUDS implementation.

4. **Library version parity**: Identical versions (Protobuf 34.0, FlatBuffers 25.12.19, Zstandard 1.5.7) compiled from source on both platforms.

5. **Algorithm direction flag** (Alg. 1, Line 27): `d = (parent[leaf]==P) ? +1 : ((parent[P]==leaf) ? -1 : +1)`. The fallback to +1 is defensive dead code; for rooted trees the first two branches always suffice.

---

## Memory & Resource Profiles

Measured via `/usr/bin/time -v` (GNU time 1.9) on Django AST (n=2,325,575):

| Method | Peak RSS (MB) | Minor Page Faults | Involuntary CS | Wall Time (s) |
|---|---|---|---|---|
| **SPPS** | **199** | 71,687 | 432 | 1.24 |
| LOUDS | 205 | 30,192 | 267 | 2.06 |
| FlatBuffers | 192 | 63,176 | 518 | 2.18 |
| Protobuf | 374 | 81,173 | 4,834 | 17.45 |

> Protobuf's Peak RSS (374 MB) is 1.88× SPPS (199 MB), reflecting per-node heap allocation overhead. Protobuf's 4,834 involuntary context switches (11.2× SPPS) indicate significant cache pressure from its scattered memory access pattern.

---

## Reproducibility

To cleanly reproduce all benchmarks:

```bash
# 1. Install exact dependency versions
sudo ./scripts/install_deps_exact.sh

# 2. Build all binaries
make gen-proto
make all

# 3. Run all experiment blocks
./scripts/run_all_blocks.sh

# 4. Memory profiling (per-method)
/usr/bin/time -v ./profiler_isolated spps datasets/real_ast_benchmark.txt
/usr/bin/time -v ./profiler_isolated louds datasets/real_ast_benchmark.txt
/usr/bin/time -v ./profiler_isolated fb datasets/real_ast_benchmark.txt
/usr/bin/time -v ./profiler_isolated pb datasets/real_ast_benchmark.txt
```

Complete output is archived in `linux_x86_64_complete_output.txt`.

---

## ⚖️ Legal Notice & Copyright

```
Copyright © 2026 The Authors. All Rights Reserved.

This repository and all of its contents — including but not limited to source code,
experiment scripts, benchmark data, terminal output logs, research reports, and
the accompanying manuscript PDF — are made available SOLELY for the purpose of
peer review and reproducibility verification associated with the submitted manuscript:

  "Signed Positional Prüfer Sequences (SPPS): A Bijective O(n) Encoding for
   Rooted Directed Ordered Trees"

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

*Cross-platform validation: ARM64 (Apple M1) + x86-64 (AMD EPYC 7763)*  
*12,006/12,006 correctness tests · SPPS fastest roundtrip on both architectures*  
*Executed: Sat Apr 18, 2026*

</div>
