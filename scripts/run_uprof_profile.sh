#!/bin/bash
# run_uprof_profile.sh — AMD uProf bottleneck decomposition
# Platform: Linux x86-64 (AMD Ryzen 5 7235HS, Zen 3+)
# Equivalent of: xctrace CPU Counters "Bottleneck Analysis" on macOS
#
# AMD uProf provides the same 4-way pipeline decomposition:
#   Retiring       = xctrace "Useful"
#   Frontend Bound = xctrace "Delivery (cache miss)"
#   Backend Bound  = xctrace "Processing (pipeline)"
#   Bad Speculation = xctrace "Discarded (speculation)"
#
# Prerequisites:
#   1. Download AMD uProf from: https://developer.amd.com/amd-uprof/
#   2. Extract and add to PATH:
#      export PATH=$PATH:/opt/AMDuProf/bin
#   3. Run as root or set: echo -1 | sudo tee /proc/sys/kernel/perf_event_paranoid
#
# Usage: sudo ./scripts/run_uprof_profile.sh

set -e
SCRIPT_DIR="$( cd "$( dirname "${BASH_SOURCE[0]}" )" && pwd )"
ROOT_DIR="$(dirname "$SCRIPT_DIR")"
cd "$ROOT_DIR"

DATASET="datasets/real_ast_benchmark.txt"
METHODS=("spps" "louds" "fb" "pb")
METHOD_NAMES=("SPPS" "LOUDS" "FlatBuffers" "Protobuf")
OUTPUT_DIR="uprof_results"

mkdir -p "$OUTPUT_DIR"

echo "================================================================="
echo " AMD uPROF PIPELINE ANALYSIS"
echo " CPU: $(lscpu | grep 'Model name' | sed 's/.*: *//')"
echo " Dataset: $DATASET"
echo " Date: $(date)"
echo "================================================================="
echo ""

# Check AMD uProf availability
if ! command -v AMDuProfCLI &> /dev/null; then
    echo "[ERROR] AMDuProfCLI not found."
    echo ""
    echo "Install AMD uProf:"
    echo "  1. Download from: https://developer.amd.com/amd-uprof/"
    echo "  2. Extract: tar xzf AMDuProf_Linux_x64_*.tar.gz"
    echo "  3. Add to PATH: export PATH=\$PATH:/path/to/AMDuProf/bin"
    echo ""
    echo "Alternatively, use perf stat for basic counters:"
    echo "  ./scripts/run_perf_profile.sh"
    exit 1
fi

# Build profiler_isolated if needed
if [ ! -f profiler_isolated ]; then
    echo "[*] Building profiler_isolated..."
    make profiler_isolated
fi

echo ""
echo "================================================================="
echo " PIPELINE ANALYSIS (assess mode)"
echo " Produces: Retiring, Frontend Bound, Backend Bound, Bad Spec"
echo "================================================================="
echo ""

for i in "${!METHODS[@]}"; do
    METHOD="${METHODS[$i]}"
    NAME="${METHOD_NAMES[$i]}"
    PROF_DIR="$OUTPUT_DIR/assess_${METHOD}"

    echo "--- $NAME ---"
    echo "  Collecting pipeline counters..."

    AMDuProfCLI collect \
        --config assess \
        --cpu-affinity 0 \
        -o "$PROF_DIR" \
        -- ./profiler_isolated "$METHOD" "$DATASET" 2>&1

    echo "  Generating report..."
    AMDuProfCLI report \
        -i "$PROF_DIR" \
        --config assess \
        -o "$OUTPUT_DIR/report_${METHOD}.csv" 2>&1

    echo "  Report: $OUTPUT_DIR/report_${METHOD}.csv"
    echo ""
done

echo ""
echo "================================================================="
echo " RESULTS SUMMARY"
echo "================================================================="
echo ""

# Display the key metrics from each report
for i in "${!METHODS[@]}"; do
    METHOD="${METHODS[$i]}"
    NAME="${METHOD_NAMES[$i]}"
    echo "--- $NAME ---"
    if [ -f "$OUTPUT_DIR/report_${METHOD}.csv" ]; then
        cat "$OUTPUT_DIR/report_${METHOD}.csv" | head -20
    else
        echo "  (report not generated)"
    fi
    echo ""
done

echo "================================================================="
echo " AMD uPROF PROFILING COMPLETE — $(date)"
echo "================================================================="
