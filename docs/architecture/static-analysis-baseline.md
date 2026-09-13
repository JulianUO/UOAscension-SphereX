# Static Analysis Baseline — Source-X Foundation Audit

This document summarizes the static analysis tooling and layering integrity baseline for **SphereServer X** (`Source-X`).

---

## 1. Directory Layering Integrity

- **Check script**: `utilities/check-layering.sh`
- **Rule**: `src/common` must not `#include` from `src/game/` or `src/sphere/`.
- **Progress**: `CUID` lookup moved to `src/game/CUIDGame.cpp`; global UO enums unified into `src/common/uo_types/uofiles_enums.h`.

---

## 2. Static Analysis Tools

### CppCheck
```bash
cmake -DUSE_CPPCHECK=ON -B build -S .
```
- **Log output**: `build/cppcheck.log`
- **Suppressions file**: `static_analysis/cppcheck-suppressions.txt`

### clang-tidy
- **Configuration**: `.clang-tidy`
- **Execution script**: `static_analysis/run-clang-tidy-config.sh` (requires `compile_commands.json`)

---

## 3. Core Audit Priority Modules

- `src/common/CDataBase.cpp` — SQL database verbs (PLEVEL_Admin gated)
- `src/common/CScriptObj.cpp` — Script engine parser capabilities & recursion bounds
- `src/network/receive.cpp` — Incoming packet handlers
- `src/network/CNetworkInput.cpp` — Socket buffer limits & input sanitization

---

## 4. Unit Test Suite (`UNIT_TESTING=ON`)

The unit test suite validates core primitives:
- `t_CSReferenceCount.cpp` — Ref counting concurrency
- `t_vendor_buy.cpp` — Vendor transaction safety (`VendorBuyHelper`)
- `t_CUID.cpp` — UID parsing and game entity identification
- `t_num_parsing.cpp`, `t_CPointBase.cpp`, `t_CUOClientVersion.cpp` — String parsing and point math
- `t_ultimalive.cpp`, `t_ultimalive_discovery.cpp` — UltimaLive streaming & fog-of-war map discovery
