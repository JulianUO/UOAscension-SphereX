#!/usr/bin/env sh
# Configure and build Source-X with AddressSanitizer, UBSan, and LeakSanitizer (Linux).
# Usage: ./utilities/build-asan-linux.sh [build_dir]
set -eu

BUILD_DIR="${1:-build-asan}"
TOOLCHAIN="${CMAKE_TOOLCHAIN_FILE:-cmake/toolchains/Linux-Clang-x86_64.cmake}"

echo "==> Configuring ASAN+UBSAN+LSAN build in ${BUILD_DIR}"
cmake -G "Ninja" \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_TOOLCHAIN_FILE="${TOOLCHAIN}" \
    -DUSE_ASAN=ON \
    -DUSE_UBSAN=ON \
    -DUSE_LSAN=ON \
    -DUNIT_TESTING=ON \
    -B "${BUILD_DIR}" \
    -S .

echo "==> Building"
cmake --build "${BUILD_DIR}"

echo "==> Done. Source sanitizer env before running:"
echo "    source utilities/configure-asan.sh"
echo "    ${BUILD_DIR}/bin-x86_64/SphereSvrX64_debug"
