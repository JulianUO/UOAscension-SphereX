#!/usr/bin/env sh
# Reproducible smoke-test checklist for memory/stability audit.
# Runs unit tests when available; documents manual server scenarios.
set -eu

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
BUILD_DIR="${BUILD_DIR:-build-asan}"
REPORT="${REPORT:-${ROOT}/utilities/audit-baseline.txt}"

echo "Source-X memory audit smoke test" | tee "${REPORT}"
echo "Date: $(date -u '+%Y-%m-%dT%H:%M:%SZ')" | tee -a "${REPORT}"
echo "Build dir: ${BUILD_DIR}" | tee -a "${REPORT}"
echo "" | tee -a "${REPORT}"

# 1. Unit tests (isolated, no live shard required)
if [ -f "${BUILD_DIR}/bin-x86_64/spheresvr_tests" ] || [ -f "${BUILD_DIR}/bin-x86_64/SphereSvrX_tests" ]; then
    echo "==> Running unit tests" | tee -a "${REPORT}"
    ctest --test-dir "${BUILD_DIR}" --output-on-failure 2>&1 | tee -a "${REPORT}" || true
elif [ -d "${BUILD_DIR}" ]; then
    echo "==> Unit test binary not found. Rebuild with -DUNIT_TESTING=ON" | tee -a "${REPORT}"
else
    echo "==> Build directory missing. Run utilities/build-asan-linux.sh first." | tee -a "${REPORT}"
fi

echo "" | tee -a "${REPORT}"
echo "==> Manual sanitizer scenarios (run server with configure-asan.sh sourced):" | tee -a "${REPORT}"
cat <<'EOF' | tee -a "${REPORT}"
  1. Startup + script load + player login
  2. Create/destroy items, containers, multis (housing)
  3. NPC vendor trade (receive.cpp vendor buy path)
  4. World save + GarbageCollection
  5. Client disconnect during combat/targeting

Watch server log for:
  - "GC: N unplaced objects!"
  - "Object memory leak X!=Y"
  - "UID conflict delete"
EOF

echo "" | tee -a "${REPORT}"
echo "Report written to ${REPORT}"
