#!/bin/bash
# run_all_blocks.sh — Build and run all experiment blocks sequentially
# Platform: Linux x86-64 (Ubuntu 22.04)
# Usage: ./scripts/run_all_blocks.sh

set -e
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

echo "============================================"
echo " SPPS Experiment Suite — Full Run"
echo " Platform: $(uname -s) $(uname -m)"
echo " Compiler: $(g++ --version | head -1)"
echo " Date: $(date)"
echo "============================================"
echo ""

# ---- Build ----
echo "[*] Building all targets..."
make clean
make all
echo "[✓] Build complete"
echo ""

# ---- Block A: Correctness ----
echo "============================================"
echo " Running Block A — Correctness (12,006 tests)"
echo "============================================"
./block_a_correctness 2>&1 | tee block_a_output.txt
echo ""

# ---- Block H: Worked Examples ----
echo "============================================"
echo " Running Block H — Worked Examples"
echo "============================================"
./block_h_worked_examples 2>&1 | tee block_h_output.txt
echo ""

# ---- Block B: Linearity ----
echo "============================================"
echo " Running Block B — O(n) Linearity Proof"
echo "============================================"
./block_b_linearity 2>&1 | tee block_b_output.txt
echo ""

# ---- Block C: Benchmarks ----
echo "============================================"
echo " Running Block C — Real Dataset Benchmarks"
echo " (4 methods × 3 datasets × 30 trials)"
echo "============================================"
./block_c_benchmarks 2>&1 | tee block_c_output.txt
echo ""

# ---- Block D: LOUDS Head-to-Head ----
echo "============================================"
echo " Running Block D — LOUDS Baseline Integration"
echo "============================================"
./block_d_louds 2>&1 | tee block_d_output.txt
echo ""

# ---- Block E: Compression ----
echo "============================================"
echo " Running Block E — Compression Experiments"
echo "============================================"
./block_e_compression 2>&1 | tee block_e_output.txt
echo ""

# ---- Block F: Tail Latency ----
echo "============================================"
echo " Running Block F — Tail Latency & Variance"
echo "============================================"
./block_f_tail_latency 2>&1 | tee block_f_output.txt
echo ""

# ---- Block G: Downstream ----
echo "============================================"
echo " Running Block G — Downstream Macro-Benchmark"
echo "============================================"
./block_g_downstream 2>&1 | tee block_g_output.txt
echo ""

# ---- Combine outputs ----
echo "============================================"
echo " Combining all outputs..."
echo "============================================"
cat block_a_output.txt block_b_output.txt block_h_output.txt \
    block_c_output.txt block_d_output.txt block_e_output.txt \
    block_f_output.txt block_g_output.txt > all_block_outputs.txt
echo "[✓] Combined output saved to: all_block_outputs.txt"

echo ""
echo "============================================"
echo " ALL BLOCKS COMPLETE"
echo " $(date)"
echo "============================================"
