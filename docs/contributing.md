# Contributing to Source-X

## Build variants

| CMAKE_BUILD_TYPE | Purpose |
|------------------|---------|
| **Release** | Production shard |
| **Nightly** | Pre-release testing; extra asserts |
| **Debug** | Development; use with sanitizers |

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Linux-GNU-x86_64.cmake \
      -DCMAKE_BUILD_TYPE=Nightly -B build -S .
cmake --build build
```

## Sanitizers

Linux (full suite):

```bash
./utilities/build-asan-linux.sh
source utilities/configure-asan.sh
./build-asan/bin-x86_64/SphereSvrX64_debug
```

Windows (ASAN only):

```bat
utilities\build-asan-windows.bat
utilities\configure-asan.bat
```

## Unit tests

```bash
cmake -DUNIT_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -B build-test -S .
cmake --build build-test
ctest --test-dir build-test --output-on-failure
```

## Layering check

```bash
./utilities/check-layering.sh
```

`src/common/` must not add new `#include` paths into `src/game/` or `src/sphere/`. Shared UO enums live in `src/common/uo_types/`.

## Static analysis

```bash
cmake -DUSE_CPPCHECK=ON -B build -S .
# After configure:
static_analysis/run-clang-tidy-config.sh   # requires compile_commands.json
```

See `docs/static-analysis-baseline.txt` for recorded baseline counts.

## Table-driven definitions (`.tbl`)

Props, triggers, and script functions are generated from `src/tables/*.tbl`:

1. Edit the relevant `.tbl` file (e.g. `CItemProps.tbl`)
2. Rebuild — tables are processed at compile time
3. Match naming conventions in adjacent `.tbl` files

## Security-related sphere.ini keys

| Key | Default | Notes |
|-----|---------|-------|
| `USEHTTP` | 0 | HTTP on game port; enable only if needed |
| `ALLOWEMPTYPASSWORDAUTOSET` | 0 | Legacy empty-account password claim |
| `MD5PASSWORDS` | 0 | Prefer bcrypt via scripts for new shards |

## Documentation

- [architecture.md](architecture.md) — globals, threading, script matrix
- [foundation-audit-baseline.md](foundation-audit-baseline.md) — audit tracking
- [memory-audit-baseline.md](memory-audit-baseline.md) — stability tooling
