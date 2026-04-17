#!/bin/bash
# run_perf_profile.sh — Microarchitectural profiling via perf stat
# Platform: Linux x86-64 (Ubuntu 22.04, AMD Ryzen 5 7235HS)
# Equivalent of: xctrace CPU Counters bottleneck analysis on macOS
#
# Produces data for:
#   - Table 4: Instructions retired, CPU cycles, IPC, Peak RSS
#   - Table 5: Bottleneck decomposition (via raw counters on AMD)
#
# Usage: sudo ./scripts/run_perf_profile.sh
# Note:  Requires perf_event_paranoid=-1 or root access

set -e
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

DATASET="datasets/real_ast_benchmark.txt"
METHODS=("spps" "louds" "fb" "pb")
METHOD_NAMES=("SPPS" "LOUDS" "FlatBuffers" "Protobuf")
OUTPUT_DIR="perf_results"

mkdir -p "$OUTPUT_DIR"

echo "================================================================="
echo " PERF STAT MICROARCHITECTURAL PROFILING"
echo " Platform: $(uname -s) $(uname -m)"
echo " CPU: $(lscpu | grep 'Model name' | sed 's/.*: *//')"
echo " Kernel: $(uname -r)"
echo " Dataset: $DATASET"
echo " Date: $(date)"
echo "================================================================="
echo ""

# Check perf availability
if ! command -v perf &> /dev/null; then
    echo "[ERROR] perf not found. Install with:"
    echo "  sudo apt install linux-tools-common linux-tools-generic linux-tools-\$(uname -r)"
    exit 1
fi

# Check perf_event_paranoid
PARANOID=$(cat /proc/sys/kernel/perf_event_paranoid 2>/dev/null || echo "unknown")
echo "[*] perf_event_paranoid = $PARANOID"
if [ "$PARANOID" != "-1" ] && [ "$PARANOID" != "0" ]; then
    echo "[WARNING] Set perf_event_paranoid=-1 for full counter access:"
    echo "  echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid"
    echo ""
fi

# Build profiler_isolated if needed
if [ ! -f profiler_isolated ]; then
    echo "[*] Building profiler_isolated..."
    make profiler_isolated
fi

echo ""
echo "================================================================="
echo " PHASE 1: BASIC COUNTERS (Table 4 equivalent)"
echo "================================================================="
echo ""

for i in "${!METHODS[@]}"; do
    METHOD="${METHODS[$i]}"
    NAME="${METHOD_NAMES[$i]}"
    OUTFILE="$OUTPUT_DIR/perf_basic_${METHOD}.txt"

    echo "--- $NAME ---"

    # Basic counters: instructions, cycles, IPC, cache misses
    perf stat -e \
        cycles:u,instructions:u,\
cache-references:u,cache-misses:u,\
L1-dcache-loads:u,L1-dcache-load-misses:u,\
branches:u,branch-misses:u \
        -o "$OUTFILE" \
        taskset -c 0 ./profiler_isolated "$METHOD" "$DATASET" 2>&1

    echo "  Output: $OUTFILE"

    # Get Peak RSS via /usr/bin/time
    /usr/bin/time -v taskset -c 0 ./profiler_isolated "$METHOD" "$DATASET" \
        2> "$OUTPUT_DIR/rss_${METHOD}.txt" > /dev/null
    RSS=$(grep "Maximum resident" "$OUTPUT_DIR/rss_${METHOD}.txt" | awk '{print $NF}')
    echo "  Peak RSS: $((RSS / 1024)) MB"
    echo ""
done

echo ""
echo "================================================================="
echo " PHASE 2: DETAILED CACHE HIERARCHY (-d -d -d)"
echo "================================================================="
echo ""

for i in "${!METHODS[@]}"; do
    METHOD="${METHODS[$i]}"
    NAME="${METHOD_NAMES[$i]}"
    OUTFILE="$OUTPUT_DIR/perf_cache_${METHOD}.txt"

    echo "--- $NAME ---"
    perf stat -d -d -d \
        -o "$OUTFILE" \
        taskset -c 0 ./profiler_isolated "$METHOD" "$DATASET" 2>&1
    echo "  Output: $OUTFILE"
    echo ""
done

echo ""
echo "================================================================="
echo " PHASE 3: RAW AMD PMU COUNTERS (Table 5 equivalent)"
echo " Bottleneck decomposition via AMD-specific events"
echo "================================================================="
echo ""

# AMD Zen 3+ specific PMU events for bottleneck decomposition
# These map to the xctrace bottleneck categories:
#   Delivery (Frontend)  → instruction cache misses + fetch stalls
#   Processing (Backend) → data cache misses + execution stalls
#   Discarded (Speculation) → branch mispredictions
#   Useful (Retiring) → retired instructions / total slots

for i in "${!METHODS[@]}"; do
    METHOD="${METHODS[$i]}"
    NAME="${METHOD_NAMES[$i]}"
    OUTFILE="$OUTPUT_DIR/perf_amd_pmu_${METHOD}.txt"

    echo "--- $NAME ---"
    perf stat -e \
        cycles:u,instructions:u,\
L1-icache-load-misses:u,\
L1-dcache-loads:u,L1-dcache-load-misses:u,\
dTLB-load-misses:u,iTLB-load-misses:u,\
branches:u,branch-misses:u,\
cache-references:u,cache-misses:u \
        -o "$OUTFILE" \
        taskset -c 0 ./profiler_isolated "$METHOD" "$DATASET" 2>&1
    echo "  Output: $OUTFILE"
    echo ""
done

echo ""
echo "================================================================="
echo " PHASE 4: SUMMARY"
echo "================================================================="
echo ""
echo "All perf results saved to: $OUTPUT_DIR/"
echo ""
echo "To view results:"
echo "  cat $OUTPUT_DIR/perf_basic_spps.txt"
echo ""
echo "To compute bottleneck ratios:"
echo "  Cache-miss ratio   = cache-misses / cache-references"
echo "  Branch-miss ratio  = branch-misses / branches"
echo "  L1D miss rate      = L1-dcache-load-misses / L1-dcache-loads"
echo ""
echo "================================================================="
echo " PROFILING COMPLETE — $(date)"
echo "================================================================="
