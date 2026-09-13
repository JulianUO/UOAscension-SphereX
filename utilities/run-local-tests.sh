#!/usr/bin/env bash
# ============================================================================
# SphereServer (Source-X) - Local Pre-Commit Unit Tests & Validation Runner
# ============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build-local-tests"

echo "[1/4] Configuring CMake project with UNIT_TESTING=ON..."
mkdir -p "${BUILD_DIR}"
cmake -S "${ROOT_DIR}" -B "${BUILD_DIR}" -DUNIT_TESTING=ON -DCMAKE_BUILD_TYPE=Nightly

echo "[2/4] Building SphereServer unit tests..."
cmake --build "${BUILD_DIR}" --config Nightly -j"$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)"

echo "[3/4] Executing unit tests (CTest)..."
ctest --test-dir "${BUILD_DIR}" --output-on-failure

echo "[4/4] Checking code layering..."
if [ -f "${SCRIPT_DIR}/check-layering.sh" ]; then
    chmod +x "${SCRIPT_DIR}/check-layering.sh"
    "${SCRIPT_DIR}/check-layering.sh"
fi

echo ""
echo "============================================================================"
echo "[SUCCESS] All local unit tests passed! Ready for commit."
echo "============================================================================"
