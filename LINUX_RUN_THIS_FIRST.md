# SPPS Cross-Platform Validation — Master Execution Prompt

You are executing the complete experimental suite for the research paper **"Signed Positional Prüfer Sequences: A Bijective O(n) Encoding for Rooted Directed Ordered Trees"** being submitted to **ESA 2026 Track E** (Engineering and Applications track of the European Symposium on Algorithms — an A* ranked conference).

## What This Is

This is a cross-platform validation. The experiments were originally run on macOS ARM64 (Apple M1). You are now re-running the identical codebase on Linux x86-64 (Ubuntu 22.04 LTS, AMD Ryzen 5 7235HS) to:
1. Confirm all deterministic results (correctness, sizes, compression) are platform-independent
2. Collect Linux-specific timing and profiling data
3. Generate two comprehensive output files documenting everything

## What You Must Do

There are **two instruction files** in this directory. Execute them **in order**:

### Step 1: Read and execute `LINUX_PROMPT_1_EXECUTE_AND_TERMINAL_OUTPUT.md`

This file tells you how to:
- Install exact-match dependencies (protobuf 34.0, flatbuffers 25.12.19, zstd 1.5.7)
- Build the entire codebase (10 C++ binaries)
- Run all 8 experiment blocks (A, B, C, D, E, F, G, H)
- Run profiling with `/usr/bin/time -v` and `perf stat` for all 4 methods
- Generate the file `SPPS_Complete_Terminal_Output.txt`

### Step 2: Read and execute `LINUX_PROMPT_2_EXHAUSTIVE_REPORT.md`

This file tells you how to:
- Analyze all collected results
- Compute derived metrics (IPC, instructions/node, throughput, etc.)
- Compare Linux results against Mac baseline values (provided in the file)
- Generate the file `SPPS_Exhaustive_Research_Report.txt` with 19 sections + 20 appendices

### Step 3: Push everything to GitHub

After both output files are generated and verified, push ALL results to the dedicated results repository.

**NOTE:** This file (`LINUX_RUN_THIS_FIRST.md`) contains an auth token and is automatically excluded from the push via `.gitignore`.

```bash
cd /path/to/experiments

# Configure git identity
git config --global user.name "Cyrax321"
git config --global user.email "cyrax8590@gmail.com"

# Initialize repo
git init

# IMPORTANT: Before git init, exclude this file so the token never gets pushed
echo "LINUX_RUN_THIS_FIRST.md" >> .gitignore

# Set remote with token authentication
git remote add origin https://github.com/Cyrax321/spps-linux-experiment-results.git

# Add everything: source code, datasets, output files, reports, profiling data
git add -A
git commit -m "Linux x86-64 cross-platform validation: all 8 experiment blocks + profiling

Platform: Ubuntu 22.04 LTS, AMD Ryzen 5 7235HS, g++ 11.x
Experiments: Blocks A-H (12,006 correctness tests, linearity, benchmarks,
  LOUDS H2H, compression, tail latency, downstream, worked examples)
Profiling: perf stat + /usr/bin/time -v for SPPS/LOUDS/FlatBuffers/Protobuf
Output files:
  - SPPS_Complete_Terminal_Output.txt
  - SPPS_Exhaustive_Research_Report.txt
  - block_*_output.txt (8 files)
  - profile_*_time.txt and profile_*_perf.txt (8 files)
Dependencies: protobuf 34.0, flatbuffers 25.12.19, zstd 1.5.7 (exact Mac match)
Paper: ESA 2026 Track E submission"

git branch -M main
git push -u origin main --force
```

Make sure the push includes:
- All `.cpp` source files (unmodified)
- All dataset files in `datasets/`
- All `block_*_output.txt` files
- All `profile_*` files
- `SPPS_Complete_Terminal_Output.txt`
- `SPPS_Exhaustive_Research_Report.txt`
- `Makefile`, `tree.proto`, `tree.fbs`
- All scripts in `scripts/`
- `LINUX_PROMPT_1_EXECUTE_AND_TERMINAL_OUTPUT.md` and `LINUX_PROMPT_2_EXHAUSTIVE_REPORT.md`

## Critical Rules

1. **Do NOT modify any `.cpp` source file.** The algorithm must be byte-for-byte identical to the Mac run.
2. **Do NOT modify or delete any dataset file** in `datasets/`.
3. **Do NOT skip any experiment block.** All 8 blocks (A-H) must run.
4. **If Block A fails** (any test out of 12,006 fails), **STOP immediately** — the build is broken.
5. **Record EVERY piece of output** — nothing should be summarized or truncated.
6. **Both output files must be generated** before you finish.

## The Paper Context

The SPPS algorithm encodes rooted, directed, ordered trees into a single integer array of length n-1 in O(n) time. It solves three gaps in classical Prüfer sequences (root loss, direction loss, sibling-order loss) through virtual root injection, bijective radix packing, and a cache-efficient sliding-pointer realization.

The paper claims:
- 12,006/12,006 correctness tests pass
- O(n) linearity with R² ≥ 0.998 across all topologies
- Fastest total roundtrip: 2.06x over LOUDS, 3.27x over FlatBuffers, 2.97x over Protobuf
- Lowest cache-miss pressure: 2.32x lower than LOUDS
- Best compression: SPPS+zstd-3 at 3.28 B/node

**ESA Track E reviewers will scrutinize the experimental methodology. Cross-platform validation on x86-64 eliminates the "single platform" limitation currently noted in the paper.**

## Start Now

Read `LINUX_PROMPT_1_EXECUTE_AND_TERMINAL_OUTPUT.md` and begin execution.
