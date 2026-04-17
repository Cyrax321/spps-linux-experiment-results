#!/bin/bash
# install_deps_exact.sh — Install EXACT matching dependency versions for SPPS
# Matches Mac Homebrew versions for reproducible cross-platform results
#
# Mac versions (verified):
#   protobuf:    34.0 (Protobuf C++ API v7.34.0)
#   flatbuffers: 25.12.19
#   zstd:        1.5.7
#   abseil:      20260107.1
#
# Usage: sudo ./scripts/install_deps_exact.sh

set -e

echo "================================================================="
echo " SPPS Dependencies — Exact Version Match Install"
echo " Target versions: protobuf=34.0, flatc=25.12.19, zstd=1.5.7"
echo " Platform: Ubuntu 22.04 LTS x86-64"
echo "================================================================="
echo ""

BUILDDIR="/tmp/spps_deps_build"
mkdir -p "$BUILDDIR"

# ---- Core build tools ----
echo "[1/6] Installing build essentials..."
apt update
apt install -y build-essential g++ pkg-config cmake git wget

# ---- Zstandard 1.5.7 (from source) ----
echo ""
echo "[2/6] Installing Zstandard 1.5.7 from source..."
cd "$BUILDDIR"
if [ ! -f "zstd-1.5.7.tar.gz" ]; then
    wget -q https://github.com/facebook/zstd/releases/download/v1.5.7/zstd-1.5.7.tar.gz
fi
tar xf zstd-1.5.7.tar.gz
cd zstd-1.5.7
make -j$(nproc)
make install
ldconfig
echo "  ✓ zstd installed: $(zstd --version 2>&1 | head -1)"

# ---- Abseil 20260107.1 (required by protobuf 34.0) ----
echo ""
echo "[3/6] Installing Abseil 20260107.1 from source..."
cd "$BUILDDIR"
if [ ! -d "abseil-cpp" ]; then
    git clone https://github.com/abseil/abseil-cpp.git
fi
cd abseil-cpp
git fetch --tags
git checkout 20260107.1 2>/dev/null || {
    echo "  [WARN] Tag 20260107.1 not found, trying latest LTS..."
    git checkout lts_2024_01_16 2>/dev/null || git checkout main
}
rm -rf build && mkdir -p build && cd build
cmake .. \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -DABSL_BUILD_TESTING=OFF \
    -DCMAKE_CXX_STANDARD=17 \
    -DABSL_PROPAGATE_CXX_STD=ON
make -j$(nproc)
make install
ldconfig
echo "  ✓ Abseil installed"

# ---- Protobuf 34.0 (from source) ----
echo ""
echo "[4/6] Installing Protobuf 34.0 from source..."
cd "$BUILDDIR"
if [ ! -f "protobuf-34.0.tar.gz" ]; then
    wget -q https://github.com/protocolbuffers/protobuf/releases/download/v34.0/protobuf-34.0.tar.gz
fi
tar xf protobuf-34.0.tar.gz
cd protobuf-34.0
rm -rf build && mkdir -p build && cd build
cmake .. \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_CXX_STANDARD=17 \
    -Dprotobuf_BUILD_TESTS=OFF \
    -DCMAKE_POSITION_INDEPENDENT_CODE=ON \
    -Dprotobuf_ABSL_PROVIDER=package
make -j$(nproc)
make install
ldconfig
echo "  ✓ protoc installed: $(protoc --version 2>&1)"

# ---- FlatBuffers 25.12.19 (from source) ----
echo ""
echo "[5/6] Installing FlatBuffers 25.12.19 from source..."
cd "$BUILDDIR"
if [ ! -f "v25.12.19.tar.gz" ]; then
    wget -q https://github.com/google/flatbuffers/archive/refs/tags/v25.12.19.tar.gz
fi
tar xf v25.12.19.tar.gz
cd flatbuffers-25.12.19
rm -rf build && mkdir -p build && cd build
cmake .. \
    -DCMAKE_INSTALL_PREFIX=/usr/local \
    -DCMAKE_CXX_STANDARD=17 \
    -DFLATBUFFERS_BUILD_TESTS=OFF
make -j$(nproc)
make install
ldconfig
echo "  ✓ flatc installed: $(flatc --version 2>&1)"

# ---- perf (Linux profiling) ----
echo ""
echo "[6/6] Installing perf..."
apt install -y linux-tools-common linux-tools-generic
apt install -y linux-tools-$(uname -r) 2>/dev/null || \
    echo "  [WARN] Could not install linux-tools-$(uname -r) — perf may not work"

# Configure perf access
echo -1 > /proc/sys/kernel/perf_event_paranoid
echo "kernel.perf_event_paranoid = -1" >> /etc/sysctl.d/99-perf.conf
sysctl -p /etc/sysctl.d/99-perf.conf 2>/dev/null || true

echo ""
echo "================================================================="
echo " Verification — All Versions"
echo "================================================================="
echo ""
echo "g++:        $(g++ --version | head -1)"
echo "protoc:     $(protoc --version 2>&1)"
echo "flatc:      $(flatc --version 2>&1)"
echo "zstd:       $(zstd --version 2>&1 | head -1)"
echo "perf:       $(perf --version 2>&1 || echo 'not found')"
echo "paranoid:   $(cat /proc/sys/kernel/perf_event_paranoid)"
echo ""

# Version validation
PROTOC_VER=$(protoc --version 2>&1 | grep -oP '\d+\.\d+$')
FLATC_VER=$(flatc --version 2>&1 | grep -oP '[\d.]+$')
ZSTD_VER=$(zstd --version 2>&1 | grep -oP 'v[\d.]+' | head -1)

echo "================================================================="
echo " Version Match Check"
echo "================================================================="
echo ""

check_version() {
    local name="$1" actual="$2" expected="$3"
    if [ "$actual" = "$expected" ]; then
        echo "  ✓ $name: $actual (matches Mac)"
    else
        echo "  ✗ $name: $actual (Mac has $expected — MISMATCH)"
    fi
}

check_version "protoc"      "$PROTOC_VER"  "34.0"
check_version "flatc"       "$FLATC_VER"   "25.12.19"
check_version "zstd"        "$ZSTD_VER"    "v1.5.7"

echo ""
echo "================================================================="
echo " Next Steps"
echo "================================================================="
echo ""
echo "  1. cd to experiment directory"
echo "  2. make gen-proto        # regenerate headers for this platform"
echo "  3. make clean && make all"
echo "  4. ./scripts/run_all_blocks.sh"
echo "  5. sudo ./scripts/run_perf_profile.sh"
echo ""
echo "Optional: Install AMD uProf for full pipeline decomposition:"
echo "  Download from: https://developer.amd.com/amd-uprof/"
echo ""
echo "================================================================="
echo " INSTALLATION COMPLETE — $(date)"
echo "================================================================="
