# Experiment Blocks

| Block | File | Purpose | Key Metric |
|-------|------|---------|------------|
| A | `block_a_correctness.cpp` | 12,006 encode-decode-verify tests | 0 failures |
| B | `block_b_linearity.cpp` | O(n) linearity proof (4 topologies) | R² ≥ 0.998 |
| C | `block_c_benchmarks.cpp` | 4 methods × 3 datasets × 30 trials | Roundtrip ms |
| D | `block_d_louds.cpp` | LOUDS head-to-head comparison | Speedup ratio |
| E | `block_e_compression.cpp` | SPPS + zstd compression | B/node |
| F | `block_f_tail_latency.cpp` | P50-P99 percentile analysis | CV% ≤ 5% |
| G | `block_g_downstream.cpp` | Downstream macro-benchmark | Pipeline ms |
| H | `block_h_worked_examples.cpp` | Step-by-step traces | Paper figures |

## Running

```bash
./scripts/run_all_blocks.sh        # All experiments
make block_a && ./block_a_correctness  # Individual block
sudo ./scripts/run_perf_profile.sh     # Microarchitectural profiling
```

## Profiling: Mac to Linux

| Mac (xctrace) | Linux (perf) |
|---|---|
| Instructions retired | `perf stat -e instructions:u` |
| CPU cycles | `perf stat -e cycles:u` |
| Delivery bottleneck | Frontend Bound (AMD uProf) |
| Processing bottleneck | Backend Bound (AMD uProf) |
| Discarded | Bad Speculation (AMD uProf) |
| Useful | Retiring (AMD uProf) |
| Peak RSS | `/usr/bin/time -v` |
