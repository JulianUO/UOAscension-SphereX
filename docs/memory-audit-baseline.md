# Memory Audit Baseline

Generated as part of the Source-X stability audit. Re-run after sanitizer builds to refresh.

## Tooling

| Step | Linux | Windows |
|------|-------|---------|
| Configure ASAN build | `./utilities/build-asan-linux.sh` | `utilities\build-asan-windows.bat` |
| Sanitizer env | `source utilities/configure-asan.sh` | `utilities\configure-asan.bat` |
| Smoke checklist | `./utilities/smoke-test.sh` | `utilities\smoke-test.bat` |
| Static analysis | `cmake -DUSE_CPPCHECK=ON ...` | same |
| clang-tidy | `static_analysis/run-clang-tidy-config.sh` | requires WSL or Linux |

## Known Critical Issues

| ID | Issue | Status |
|----|-------|--------|
| C1 | Vendor buy OOB when item slots full | **Fixed** — `receive.cpp` |
| C2 | ASSERT stripped in Release/Nightly | **Fixed** — null checks in `send.cpp` |
| C3 | EXC_TRY no-op without `_EXCEPTIONS_DEBUG` | **Mitigated** — enabled in Nightly toolchains |
| H1 | Unguarded `GetChar()->` | **Fixed** — `CClientMsg.cpp`, `send.cpp` |
| H2 | Unguarded `ItemFind()` + cast | **Fixed** — `CItemMulti*.cpp`, `CClientMsg.cpp` |
| M1 | `CSReferenceCounted::operator=` leak | **Fixed** — `CSReferenceCount.h` |

## Runtime GC Signals

Monitor server log during smoke scenarios:

- `GC: N unplaced objects!` — objects created but never placed
- `Object memory leak X!=Y` — UID table vs `CObjBase::sm_iCount` mismatch
- `UID conflict delete` — direct delete outside deferred GC path

## Manual Smoke Scenarios

1. Startup + script load + player login
2. Create/destroy items, containers, multis (housing)
3. NPC vendor trade
4. World save + GarbageCollection
5. Client disconnect during combat/targeting

## Unit Tests

Build with `-DUNIT_TESTING=ON` and run `ctest --test-dir build-asan --output-on-failure`.
