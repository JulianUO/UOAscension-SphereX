# Modern C++20 Standard & Concurrency Architecture

**SphereServer X** (`Source-X`) has undergone major core refactoring to modernize the codebase to the **C++20 standard**, replace legacy custom reference counts with standard RAII memory containers, and introduce thread-safe synchronization primitives.

---

## 1. C++20 Toolchain Requirements

SphereServer X requires modern C++20 compliant compilers:
- **Microsoft Visual Studio**: VS 2019 version 16.11 or newer (MSVC 19.29+)
- **GCC**: GCC 8.0 or newer
- **Clang**: Clang 10.0 or newer
- **Build System**: CMake 3.16+

---

## 2. Key Refactoring & Safety Features

### 2.1 Memory Management & Smart Pointers

- Legacy raw heap buffer allocations for network input/output buffers, compression blocks, and decryption states were migrated to `std::unique_ptr<byte[]>`.
- Network packet buffers (`m_encryptBuffer`, `m_receiveBuffer`, `m_decryptBuffer`) automatically manage lifetime via standard RAII wrappers, eliminating memory leaks on disconnected client sockets.

### 2.2 Concurrency & Thread Synchronization

- Engine refactoring introduced standard C++ `std::mutex`, `std::lock_guard`, and `std::scoped_lock` primitives for multi-threaded subsystems (such as `CSessionRegistry` and `CInternalApiServer`).
- Atomic reference counters (`CSReferenceCount`) updated with standard memory order semantics (`std::memory_order_relaxed`, `std::memory_order_acq_rel`).

### 2.3 Strict Directory Layering Enforcement

The codebase enforces a strict topological dependency layering:

```
src/common/ (Types, Strings, Utilities)
    ↓
src/game/ (World, Chars, Items, Sectors)
    ↓
src/network/ & src/sphere/ (Sockets, Entry Point)
```

- **Invariant**: Code inside `src/common/` must **NEVER** `#include` headers from `src/game/` or `src/sphere/`.
- Layer integrity is continuously verified in CI using `utilities/check-layering.sh`.

---

## 3. Related Source Files & Tooling

- `CMakeLists.txt` — Modern CMake `target_compile_features(SphereServer PUBLIC cxx_std_20)` target configuration
- `utilities/check-layering.sh` — Automated directory layering integrity audit script
- `src/common/sphere_library/CSReferenceCount.h` — Atomic reference count wrapper
- `docs/architecture/foundation-audit-baseline.md` & `memory-audit-baseline.md` — Baseline audit documents
