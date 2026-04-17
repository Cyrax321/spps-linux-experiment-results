#!/bin/bash
# install_deps.sh — Install all dependencies for SPPS experiments on Ubuntu 22.04
# Usage: sudo ./scripts/install_deps.sh

set -e

echo "============================================"
echo " SPPS Dependencies — Ubuntu 22.04"
echo "============================================"
echo ""

# Core build tools
echo "[1/6] Installing build essentials..."
apt update
apt install -y build-essential g++ pkg-config cmake

# Protocol Buffers
echo "[2/6] Installing Protocol Buffers..."
apt install -y protobuf-compiler libprotobuf-dev

# FlatBuffers
echo "[3/6] Installing FlatBuffers..."
apt install -y libflatbuffers-dev flatbuffers-compiler

# Zstandard
echo "[4/6] Installing Zstandard..."
apt install -y libzstd-dev

# perf (Linux profiling)
echo "[5/6] Installing perf..."
apt install -y linux-tools-common linux-tools-generic
# Try kernel-specific tools (may fail in some environments)
apt install -y linux-tools-$(uname -r) 2>/dev/null || \
    echo "  [WARN] Could not install linux-tools-$(uname -r) — perf may not work"

# Configure perf access
echo "[6/6] Configuring perf_event_paranoid..."
echo -1 > /proc/sys/kernel/perf_event_paranoid
echo "kernel.perf_event_paranoid = -1" >> /etc/sysctl.d/99-perf.conf
sysctl -p /etc/sysctl.d/99-perf.conf 2>/dev/null || true

echo ""
echo "============================================"
echo " Verification"
echo "============================================"
echo ""
echo "g++:        $(g++ --version | head -1)"
echo "protoc:     $(protoc --version)"
echo "flatc:      $(flatc --version 2>/dev/null || echo 'not found')"
echo "perf:       $(perf --version 2>/dev/null || echo 'not found')"
echo "paranoid:   $(cat /proc/sys/kernel/perf_event_paranoid)"
echo ""
echo "[✓] All dependencies installed successfully"
echo ""
echo "Optional: Install AMD uProf for full pipeline decomposition:"
echo "  Download from: https://developer.amd.com/amd-uprof/"
echo ""
echo "Next steps:"
echo "  1. cd to experiment directory"
echo "  2. make gen-proto    # regenerate protobuf/flatbuf headers"
echo "  3. make all          # build everything"
echo "  4. ./scripts/run_all_blocks.sh     # run experiments"
echo "  5. sudo ./scripts/run_perf_profile.sh  # microarchitectural profiling"
