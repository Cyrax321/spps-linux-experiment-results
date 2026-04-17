# Platform Specification

## Target Machine (Linux)

| Spec | Value |
|------|-------|
| Machine | Lenovo LOQ 15 |
| CPU | AMD Ryzen 5 7235HS (4C/8T, Zen 3+, 3.2-4.2 GHz) |
| L2 Cache | 2 MB |
| L3 Cache | 8 MB |
| RAM | 12 GB DDR5-4800 (1x SO-DIMM) |
| Storage | 512 GB NVMe PCIe 4.0x4 |
| GPU | NVIDIA RTX 3050A 4GB (not used) |
| OS | Ubuntu 22.04 LTS |
| Compiler | g++ 11.x, -std=c++17 -O3 -march=native |
| Profiling | perf stat + AMD uProf |

## Previous Machine (Mac — reference only)

| Spec | Value |
|------|-------|
| CPU | Apple M1 (4 Firestorm, 3.2 GHz) |
| L1D Cache | 128 KB |
| L2 Cache | 12 MB shared |
| RAM | 8 GB LPDDR4X (68.25 GB/s) |
| OS | macOS 26.2 |
| Compiler | Apple Clang 17, -std=c++17 -O3 |
| Profiling | xctrace CPU Counters |

## Key Architecture Differences

The SPPS cache-miss advantage is algorithmic (sequential access pattern),
not architecture-specific. Expected to replicate on AMD Zen 3+.

AMD Zen 3+ has different L2/L3 cache sizes and prefetcher heuristics than
Apple M1, which may affect absolute timings but not relative speedups.
