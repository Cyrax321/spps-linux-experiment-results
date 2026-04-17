# PROMPT 1: Execute All SPPS Experiments and Generate Terminal Output Log

You are executing the complete SPPS (Signed Positional Prüfer Sequences) experimental suite on Linux for an ESA 2026 Track E conference paper. These experiments were originally run on macOS (Apple M1, ARM64). You are performing a cross-platform validation on Linux (x86-64).

**ABSOLUTE RULES:**
1. Do NOT modify any `.cpp` source file. The algorithm must remain byte-for-byte identical.
2. Do NOT modify or delete any dataset file in `datasets/`.
3. Do NOT skip any experiment block.
4. If Block A correctness tests fail, STOP immediately — the build is broken.
5. Record EVERY piece of terminal output — nothing should be summarized or truncated.

---

## PHASE 0: SYSTEM INFORMATION

Run and record ALL of the following:

```bash
echo "=== OS ===" && uname -a && cat /etc/os-release | head -5
echo "=== CPU ===" && cat /proc/cpuinfo | grep "model name" | head -1 && lscpu | grep -E "Thread|Core|Socket|MHz|GHz|cache|Architecture"
echo "=== RAM ===" && free -h | head -2
echo "=== KERNEL ===" && uname -r
```

Save this output — it goes into Section 1 of the terminal output file.

---

## PHASE 1: INSTALL EXACT-MATCH DEPENDENCIES

The Mac used these EXACT versions. You MUST match them precisely.

### Required versions (from Mac Homebrew):
- **Protobuf**: `libprotoc 34.0` (Protobuf C++ API v7.34.0)
- **FlatBuffers**: `flatc version 25.12.19`
- **Zstandard**: `v1.5.7`
- **Abseil**: `20260107.1` (required by protobuf 34.0)

### Why exact versions matter:
- `tree.pb.h` contains `#if PROTOBUF_VERSION != 7034000` — won't compile against other versions
- `tree_generated.h` has `static_assert(FLATBUFFERS_VERSION_MAJOR == 25 && FLATBUFFERS_VERSION_MINOR == 12 && FLATBUFFERS_VERSION_REVISION == 19)` — compile failure if mismatched
- zstd compression is deterministic per-version — different versions produce different compressed byte counts, breaking Table 6 reproducibility

### Install from source:

```bash
sudo apt update && sudo apt install -y build-essential g++ pkg-config cmake git wget

# 1. Zstandard 1.5.7
cd /tmp && wget https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz
tar xf zstd-1.5.7.tar.gz && cd zstd-1.5.7
make -j$(nproc) && sudo make install && sudo ldconfig

# 2. Abseil 20260107.1
cd /tmp && git clone https://github.com/abseil/abseil-cpp.git && cd abseil-cpp
git checkout 20260107.1 2>/dev/null || git checkout lts_2024_01_16
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
  -DABSL_BUILD_TESTING=OFF -DCMAKE_CXX_STANDARD=17 -DABSL_PROPAGATE_CXX_STD=ON
make -j$(nproc) && sudo make install && sudo ldconfig

# 3. Protobuf 34.0
cd /tmp && wget https://github.com/protocolbuffers/protobuf/releases/download/v34.0/protobuf-34.0.tar.gz
tar xf protobuf-34.0.tar.gz && cd protobuf-34.0
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_CXX_STANDARD=17 \
  -Dprotobuf_BUILD_TESTS=OFF -DCMAKE_POSITION_INDEPENDENT_CODE=ON -Dprotobuf_ABSL_PROVIDER=package
make -j$(nproc) && sudo make install && sudo ldconfig

# 4. FlatBuffers 25.12.19
cd /tmp && wget https://github.com/google/flatbuffers/archive/refs/tags/v25.12.19.tar.gz
tar xf v25.12.19.tar.gz && cd flatbuffers-25.12.19
mkdir -p build && cd build
cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DCMAKE_CXX_STANDARD=17 -DFLATBUFFERS_BUILD_TESTS=OFF
make -j$(nproc) && sudo make install && sudo ldconfig

# 5. perf
sudo apt install -y linux-tools-common linux-tools-generic
sudo apt install -y linux-tools-$(uname -r) 2>/dev/null || true
echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid
```

### Verify versions (MANDATORY — do not proceed if any mismatch):
```bash
protoc --version          # MUST show: libprotoc 34.0
flatc --version           # MUST show: flatc version 25.12.19
zstd --version            # MUST show: v1.5.7
g++ --version | head -1   # Record whatever version
```

---

## PHASE 2: BUILD

```bash
cd /path/to/experiments   # Navigate to experiment directory

# Regenerate protobuf/flatbuffers headers for Linux
make gen-proto

# Clean build
make clean && make all
```

Must compile 10 binaries with ZERO errors. The Makefile uses `g++ -std=c++17 -O3 -march=native -I.`

---

## PHASE 3: RUN ALL EXPERIMENT BLOCKS

Run each block sequentially. Close other applications. Consider using `taskset -c 0` to pin to a single core for consistent timing.

### Block A — Correctness (12,006 tests)
```bash
./block_a_correctness 2>&1 | tee block_a_output.txt
```
**MANDATORY CHECK:** Must show `12006/12006 PASS` and `OVERALL: ALL BLOCKS PASS`. If ANY test fails, STOP.

Expected output structure (must match Mac structure):
- A1: General Fuzzing — 10,000/10,000 PASS
- A2: Directed Edge Stress — 1,000/1,000 PASS
- A3: Sibling Order Stress — 1,000/1,000 PASS
- A4: Worked Example (n=9) — Sequence: [22, 33, 55, 23, 11, 77, 34, 12], PASS
- A5: Boundary Cases — n=1,2,3 all PASS

### Block B — O(n) Linearity
```bash
./block_b_linearity 2>&1 | tee block_b_output.txt
```
Expected: 4 topologies (Path, Star, Balanced Binary, Random AST-Like), each at 8-12 scale points, 30 trials each. R² values should all be ≥ 0.998. ns/node values WILL differ from Mac.

### Block H — Worked Examples
```bash
./block_h_worked_examples 2>&1 | tee block_h_output.txt
```
**MANDATORY CHECK:** Sequence must be exactly `S = [18, 19, 9, 54, 27, 10]`. This is mathematically deterministic.

### Block C — Real Dataset Benchmarks
```bash
./block_c_benchmarks 2>&1 | tee block_c_output.txt
```
Expected: 4 methods × 3 datasets × 30 trials. Size values MUST be deterministic: SPPS=8.00 B/node, LOUDS=0.25 B/node, FB=20.00 B/node.

### Block D — LOUDS Head-to-Head
```bash
./block_d_louds 2>&1 | tee block_d_output.txt
```
Expected: Django AST only, 30 trials, all 4 methods. SPPS should have fastest total roundtrip.

### Block E — Compression
```bash
./block_e_compression 2>&1 | tee block_e_output.txt
```
Expected: E1 (3 datasets + zstd), E2 (4 topologies), E3 (all-format comparison). Raw SPPS = 8.00 B/node always.
If zstd 1.5.7 installed: Django SPPS+zstd-3 compressed = 7,638,651 bytes (3.28 B/node).

### Block F — Tail Latency
```bash
./block_f_tail_latency 2>&1 | tee block_f_output.txt
```
Expected: P50/P90/P95/P99/Max/CV% for 3 datasets. CV% should be ≤ 5% for 500K-scale datasets.

### Block G — Downstream Macro-Benchmark
```bash
./block_g_downstream 2>&1 | tee block_g_output.txt
```
Expected: SPPS vs Protobuf pipeline. Both must find max depth = 28.

---

## PHASE 4: PROFILING

For each method, run BOTH `/usr/bin/time -v` (for RSS) and `perf stat` (for hardware counters):

```bash
# SPPS
/usr/bin/time -v ./profiler_isolated spps datasets/real_ast_benchmark.txt 2>&1 | tee profile_spps_time.txt
sudo perf stat -e instructions,cycles,cache-misses,cache-references,branch-misses,branches \
  ./profiler_isolated spps datasets/real_ast_benchmark.txt 2>&1 | tee profile_spps_perf.txt

# LOUDS
/usr/bin/time -v ./profiler_isolated louds datasets/real_ast_benchmark.txt 2>&1 | tee profile_louds_time.txt
sudo perf stat -e instructions,cycles,cache-misses,cache-references,branch-misses,branches \
  ./profiler_isolated louds datasets/real_ast_benchmark.txt 2>&1 | tee profile_louds_perf.txt

# FlatBuffers
/usr/bin/time -v ./profiler_isolated fb datasets/real_ast_benchmark.txt 2>&1 | tee profile_fb_time.txt
sudo perf stat -e instructions,cycles,cache-misses,cache-references,branch-misses,branches \
  ./profiler_isolated fb datasets/real_ast_benchmark.txt 2>&1 | tee profile_fb_perf.txt

# Protobuf
/usr/bin/time -v ./profiler_isolated pb datasets/real_ast_benchmark.txt 2>&1 | tee profile_pb_time.txt
sudo perf stat -e instructions,cycles,cache-misses,cache-references,branch-misses,branches \
  ./profiler_isolated pb datasets/real_ast_benchmark.txt 2>&1 | tee profile_pb_perf.txt
```

Also run the full profiling script if available:
```bash
sudo ./scripts/run_perf_profile.sh 2>&1 | tee perf_full_output.txt
```

---

## PHASE 5: GENERATE `SPPS_Complete_Terminal_Output.txt`

Create the file `SPPS_Complete_Terminal_Output.txt` with the EXACT structure below. Insert actual measured values in every blank space. Do NOT leave any value empty. Do NOT truncate any output.

```
==============================================================================
 SPPS EXPERIMENT SUITE - COMPLETE TERMINAL OUTPUT LOG
 Generated: 
 Platform: 
 CPU: 
 RAM: 
 Primary Dataset: Real Django AST (n=2,325,575)
 Cross-Platform Validation: Linux x86-64 run
 Original Platform: Darwin arm64 (Apple M1, 8 GB, macOS 26.2)
==============================================================================


===============================================================================
                    SECTION 1: SYSTEM INFORMATION
===============================================================================

$ uname -a


$ cat /proc/cpuinfo | grep "model name" | head -1


$ lscpu | grep -E "cache|Thread|Core|Socket"


$ free -h


$ g++ --version


$ protoc --version
libprotoc 34.0

$ flatc --version
flatc version 25.12.19

$ zstd --version
*** Zstandard CLI (64-bit) v1.5.7, by Yann Collet ***

Compilation flags:
  g++ -std=c++17 -O3 -march=native -I.
  $(pkg-config --cflags --libs protobuf) -lflatbuffers -lzstd


===============================================================================
                    SECTION 2: DATASETS
===============================================================================

$ wc -l datasets/real_ast_benchmark.txt
 2325576 datasets/real_ast_benchmark.txt

$ head -1 datasets/real_ast_benchmark.txt
2325575

Dataset 1: Django AST (n=2,325,575 nodes, depth=28)
  File: datasets/real_ast_benchmark.txt

Dataset 2: sqlite3 AST (n=503,141 nodes)
  File: datasets/sqlite3_ast_edges.txt

Dataset 3: XMark XML (n=500,000 nodes)
  File: datasets/xmark_edges.txt


===============================================================================
                    SECTION 3: BLOCK A - CORRECTNESS
===============================================================================

$ ./block_a_correctness
Paste the COMPLETE, VERBATIM output of ./block_a_correctness here.
 Do NOT summarize. Include every line including progress indicators.
 Expected structure:

=================================================================
 BLOCK A - SPPS CORRECTNESS PROOFS
 Pure C++ - No external dependencies
=================================================================

========== A1: GENERAL FUZZING (10,000 random trees) ==========
  Progress: 1000/10000
  Progress: 2000/10000
  ... (all 10 progress lines)
  RESULT: 10000/10000 passed (0 failed)
  STATUS: PASS

========== A2: DIRECTED EDGE STRESS TEST (1,000 trees) ==========
  ... (all 5 progress lines)
  RESULT: 1000/1000 passed (0 failed)
  STATUS: PASS

========== A3: SIBLING ORDER STRESS TEST (1,000 trees) ==========
  ... (all 5 progress lines)
  RESULT: 1000/1000 passed (0 failed)
  STATUS: PASS

========== A4: DIRECTED EDGE WORKED EXAMPLE (n=9) ==========
  Input tree (n=9, root=1):
    1 -> {2, 3}
    2 -> {4, 5}
    3 -> {6, 7}
    5 -> {8}
    7 -> {9}

  Initial degrees (augmented tree, v_virt=10, N=11):
    ... (all degree values)

  ENCODING TRACE:
  Step  Leaf  P     k     d     omega           Formula
  ----------------------------------------------------------------------
  1     4     2     0     1     22              1 * (2 * 11 + 0) = 22
  2     6     3     0     1     33              1 * (3 * 11 + 0) = 33
  3     8     5     0     1     55              1 * (5 * 11 + 0) = 55
  4     5     2     1     1     23              1 * (2 * 11 + 1) = 23
  5     2     1     0     1     11              1 * (1 * 11 + 0) = 11
  6     9     7     0     1     77              1 * (7 * 11 + 0) = 77
  7     7     3     1     1     34              1 * (3 * 11 + 1) = 34
  8     3     1     1     1     12              1 * (1 * 11 + 1) = 12
  9     1     10    0     -1    -110            -1 * (10 * 11 + 0) = -110

  Direction counts: d=+1: 8, d=-1: 1
  Final sequence (length 8): [22, 33, 55, 23, 11, 77, 34, 12]

  DECODING...
  Decoded root: 1
  Decoded edges:
    1 -> {2, 3}
    2 -> {4, 5}
    3 -> {6, 7}
    5 -> {8}
    7 -> {9}

  STATUS: PASS

========== A5: BOUNDARY CASES ==========
  n=1: sequence length = 0 (expected 0) PASS
  n=2: ... PASS
  n=3 chain: ... PASS
  n=3 star: ... PASS
  OVERALL: ALL PASS

=================================================================
 BLOCK A SUMMARY
=================================================================
  A1 General Fuzzing:        10000/10000 PASS
  A2 Directed Edge Stress:   1000/1000  PASS
  A3 Sibling Order Stress:   1000/1000  PASS
  A4 Worked Example (n=9):   PASS
  A5 Boundary Cases:         PASS
  OVERALL: ALL BLOCKS PASS
=================================================================
]


===============================================================================
                    SECTION 4: BLOCK B - O(n) LINEARITY PROOF
===============================================================================

$ ./block_b_linearity
Paste COMPLETE verbatim output of ./block_b_linearity here. Must include all 4 topologies
(B1: Path Graph, B2: Star Graph, B3: Balanced Binary, B4: Random AST-Like),
all scale points with ns/node values, regression equations, R² values,
and the B5 summary table. Include the NOTE about cache boundary if present.]


===============================================================================
                    SECTION 5: BLOCK H - WORKED EXAMPLES
===============================================================================

$ ./block_h_worked_examples
Paste COMPLETE verbatim output of ./block_h_worked_examples here. Must include the H1 paper example
with n=7 tree, augmented tree with v_virt=8 and N=9, the complete encoding
trace table (all 6 steps showing leaf/P/k/d/omega), the final sequence
S = [18, 19, 9, 54, 27, 10], the decoding trace, and the verification result.]


===============================================================================
      SECTION 6: BLOCK C - BENCHMARKS (Arena-Enabled Protobuf)
===============================================================================

$ ./block_c_benchmarks
Paste COMPLETE verbatim output of ./block_c_benchmarks here. Must include all 3 datasets
(Django AST C1, sqlite3 AST C2, XMark XML C3), each with the 4-method
table showing Enc/Dec/DFS/Total/Size/B-node, plus the C4 cross-dataset
stability table with CV% values.]


===============================================================================
         SECTION 7: BLOCK D - LOUDS BASELINE (Arena-Enabled)
===============================================================================

$ ./block_d_louds
Paste COMPLETE verbatim output of ./block_d_louds here. Must include the D2 head-to-head
comparison table and D3 speedup narrative with total roundtrip values
and speedup ratios.]


===============================================================================
         SECTION 8: BLOCK E - COMPRESSION
===============================================================================

$ ./block_e_compression
Paste COMPLETE verbatim output of ./block_e_compression here. Must include E1 (3 datasets with
zstd-1 and zstd-3), E2 (4 topologies at n=1M), and E3 (all-format
compression comparison: SPPS vs Protobuf vs FlatBuffers through zstd).]


===============================================================================
                    SECTION 9: BLOCK F - TAIL LATENCY
===============================================================================

$ ./block_f_tail_latency
Paste COMPLETE verbatim output of ./block_f_tail_latency here. Must include per-dataset tail
latency tables (P50/P90/P95/P99/Max/P99-P50/CV%) for all 3 datasets
and the F2 cross-dataset CV% stability table.]


===============================================================================
         SECTION 10: BLOCK G - DOWNSTREAM MACRO-BENCHMARK
===============================================================================

$ ./block_g_downstream
Paste COMPLETE verbatim output of ./block_g_downstream here. Must include the G1 downstream
pipeline results table showing SPPS vs PB(Arena) for Encode/Decode/DFS/
Total/MaxDepth/Size, plus stability CV% and speedup narrative.]


===============================================================================
          SECTION 11: MEMORY PROFILING (/usr/bin/time -v + perf stat)
===============================================================================

Platform: , , 
Dataset: Django AST, n=2,325,575
Protobuf: google::protobuf::Arena (contiguous allocation)
LOUDS: O(1) rank/select (64-bit blocks, __builtin_popcountll)

--- METHOD: spps ---


Derived metrics:
  Mean iter time:  ms
  IPC: 
  Instructions/node: 
  Cycles/node: 
  Peak RSS:  MB
  Cache miss rate: %
  Branch miss rate: %

--- METHOD: louds ---


Derived metrics:
  Mean iter time:  ms
  IPC: 
  Instructions/node: 
  Cycles/node: 
  Peak RSS:  MB
  Cache miss rate: %
  Branch miss rate: %

--- METHOD: fb ---


Derived metrics:
  Mean iter time:  ms
  IPC: 
  Instructions/node: 
  Cycles/node: 
  Peak RSS:  MB
  Cache miss rate: %
  Branch miss rate: %

--- METHOD: pb ---


Derived metrics:
  Mean iter time:  ms
  IPC: 
  Instructions/node: 
  Cycles/node: 
  Peak RSS:  MB
  Cache miss rate: %
  Branch miss rate: %

SUMMARY TABLE:
Method       Peak RSS (MB)  Instructions     Cycles          IPC    Inst/node  Cycles/node
------------------------------------------------------------------------------------------
SPPS                                             
LOUDS                                            
FlatBuffers                                      
Protobuf                                         


===============================================================================
                    SECTION 12: FAIRNESS NOTES
===============================================================================

1. PROTOBUF ARENA ALLOCATION
   All Protobuf benchmarks use google::protobuf::Arena for contiguous memory.
   Applied to: block_c, block_d, block_e, block_g, profiler_isolated.

2. APPLES-TO-APPLES COMPRESSION (Block E, Section E3)
   All three formats pass through the identical zstd compression pipeline.

3. LOUDS O(1) ACCELERATION
   LOUDS uses __builtin_popcountll for O(1) rank/select over packed 64-bit blocks.

4. OPTIMIZATION FLAGS
   g++ -std=c++17 -O3 -march=native (best available instructions for this CPU).

5. EXACT DEPENDENCY VERSIONS
   protobuf 34.0, flatbuffers 25.12.19, zstd 1.5.7 — matching Mac versions.

6. ALGORITHM UNCHANGED
   No .cpp source files were modified. `git status` confirms clean working tree.

7. ALGORITHM NOTE (Alg. 1, Line 27)
   Direction flag d uses a ternary check:
     d = (parent[leaf]==P) ? +1 : ((parent[P]==leaf) ? -1 : +1)
   The fallback to +1 is defensive; for rooted trees the first two branches
   suffice.


===============================================================================
                    SECTION 13: SUMMARY OF ALL RESULTS
===============================================================================

Table 1: Head-to-Head (Django AST, n=2,325,575, 30 trials)
LOUDS: O(1) rank/select accelerated

Method       Enc(ms)  Dec(ms)  DFS(ms)  Total(ms)  Size(B)     B/node  Peak RSS
---------------------------------------------------------------------------------
SPPS                       18,604,592  8.00     MB
LOUDS                      581,394     0.25     MB
FlatBuffers                46,511,508  20.00    MB
Protobuf                   14,246,489  6.13     MB

SPPS speedup over accelerated LOUDS: x
SPPS speedup over FlatBuffers: x
SPPS speedup over Protobuf: x

Table 2: Cross-Dataset Results (from Block C)


Table 3: Compression (Django AST, apples-to-apples zstd-3)

Format              Raw B/node   zstd-1 B/n   zstd-3 B/n   Ratio
-----------------------------------------------------------------
SPPS                8.00                       x
Protobuf (Arena)    6.13                       x
FlatBuffers         20.00                      x

Table 4: Downstream Pipeline (Block G, 30 trials)

Metric               SPPS         PB (Arena)   Speedup
-------------------------------------------------------
Encode (ms)                        x
Decode (ms)                        x
DFS Max-Depth (ms)                 x
Total Pipeline (ms)                x
Max Depth Found             

Table 5: Cross-Dataset Stability (ns/node encode)

Method       Django AST   sqlite3 AST  XMark XML   CV%
--------------------------------------------------------
SPPS                             
LOUDS                            
FlatBuffers                      
Protobuf                         

Table 6: Tail Latency (CV%)

Dataset         Encode CV%   TotalRead CV%
--------------------------------------------
Django AST             
sqlite3 AST            
XMark XML              
Mean                   

Table 7: Correctness

Block A: /12,006 tests PASS
  A1 General Fuzzing:       /10,000
  A2 Directed Edge Stress:  /1,000
  A3 Sibling Order Stress:  /1,000
  A4 Worked Example (n=9):  
  A5 Boundary Cases:        

Table 8: Linearity (Block B, 4 topologies)

Topology      Slope (ns/n)  R²        Linear?
Path Graph                
Star Graph                
Bal. Binary               
AST-Like                  

Table 9: Memory & Microarchitectural Profile

Method       Peak RSS (MB)  Instructions (B)  Cycles (B)  IPC   Inst/node  Cycles/node
---------------------------------------------------------------------------------------
SPPS                                         
LOUDS                                        
FlatBuffers                                  
Protobuf                                     


===============================================================================
 CROSS-PLATFORM COMPARISON (Mac ARM64 vs Linux x86-64)
===============================================================================

                        Mac (Apple M1)       Linux ()
                        macOS 26.2           
                        Clang 21.0.0         g++ 
------------------------------------------------------------------------
Block A Correctness     12,006/12,006 PASS   /12,006 PASS
Block H Sequence        [18,19,9,54,27,10]   
SPPS raw B/node         8.00                 
LOUDS raw B/node        0.25                 
FB raw B/node           20.00                
PB raw B/node           6.13                 
zstd-3 comp. bytes      7,638,651            
SPPS total (Django)     65.82 ms              ms
SPPS vs LOUDS           2.06x                x
SPPS vs FB              3.27x                x
SPPS vs PB              2.97x                x
Best R² (Block B)       0.999939             
Worst R² (Block B)      0.998267             
Max depth (Block G)     28                   

Deterministic checks:
  Correctness: [IDENTICAL / DIFFERS]
  Worked example: [IDENTICAL / DIFFERS]
  Raw sizes: [IDENTICAL / DIFFERS]
  Compression (zstd 1.5.7): [IDENTICAL / DIFFERS]

Performance checks:
  Speedup ratios preserved: [YES / NO]
  R² linearity preserved: [YES / NO]
  SPPS fastest on Django: [YES / NO]


===============================================================================
 END OF TERMINAL OUTPUT
===============================================================================
 All data collected on  (, ),
 single-threaded, -O3 -march=native optimized.
 Django AST: real codebase AST (n = 2,325,575).
 No algorithmic modifications to SPPS.
 Report generated .
```

---

## VERIFICATION CHECKLIST (must complete before finishing)

Run `git status` to confirm no source files were modified.

| # | Check | Expected | Status |
|---|---|---|---|
| 1 | Block A: 12,006 PASS | Must match Mac |  |
| 2 | Block H: S = [18,19,9,54,27,10] | Must match Mac |  |
| 3 | SPPS raw = 8.00 B/node | Must match Mac |  |
| 4 | LOUDS raw = 0.25 B/node | Must match Mac |  |
| 5 | FB raw = 20.00 B/node | Must match Mac |  |
| 6 | PB raw ≈ 6.13 B/node | Should match Mac |  |
| 7 | All R² ≥ 0.998 | Must hold |  |
| 8 | zstd-3 Django = 7,638,651 B | If zstd 1.5.7 |  |
| 9 | Block G max depth = 28 | Must match |  |
| 10 | SPPS fastest on Django | Must hold |  |
| 11 | Both output files exist | Must exist |  |
| 12 | No .cpp files modified | `git status` clean |  |

**After this prompt is complete, proceed to PROMPT 2 to generate the Exhaustive Research Report.**
