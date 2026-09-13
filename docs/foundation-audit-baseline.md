# Foundation Audit Baseline

Tracks findings from the comprehensive foundation audit (architecture, security, quality, memory, CI).

## Status legend

- **Fixed** — code change merged
- **Mitigated** — partial fix or config/docs
- **Open** — not yet addressed
- **Deferred** — long-term / out of scope for current phase

## A. Architecture and maintainability

| ID | Finding | Status |
|----|---------|--------|
| A1 | Inverted layering (`common` → `game` includes) | **Mitigated** — `uo_types` extraction, `CUIDGame.cpp`, layering CI |
| A2 | No architecture doc | **Fixed** — `docs/architecture.md` |
| A3 | Monolith source files | **Deferred** |
| A4 | Game enums in common | **Fixed** — `src/common/uo_types/uofiles_enums.h` |
| A5 | Legacy type conventions | **Deferred** |
| A6 | `.tbl` workflow undocumented | **Fixed** — `docs/contributing.md` |

## B. Security

| ID | Finding | Status |
|----|---------|--------|
| B1 | Raw SQL from scripts | **Fixed** — Admin priv gate on DB/SQLite verbs |
| B2 | HTTP default + guest web pages | **Fixed** — `USEHTTP` default 0; web `PLEVEL_Player` default |
| B3 | Unbounded incoming packet buffer | **Fixed** — 128 KiB cap in `CNetworkInput.cpp` |
| B4 | `pSrc = &g_Serv` on ACCOUNT ref in triggers | **Fixed** — removed privilege escalation |
| B5 | MD5 passwords; empty password auto-set | **Mitigated** — `OF_NoAutoAccountPassword` config flag |
| B6 | Gump input skips `CanUsePrivVerb` | **Fixed** — `PacketGumpValueInputResponse` |
| B7 | SYSCMD gated only by config | **Open** — document in architecture matrix |

## C. Code quality

| ID | Finding | Status |
|----|---------|--------|
| C1 | ASSERT no-op in Release | **Mitigated** — explicit null checks on network paths |
| C2 | Macro control flow | **Deferred** |
| C3 | Legacy containers | **Deferred** |
| C4 | Broad cppcheck suppressions | **Open** |

## D. Memory and stability

See [memory-audit-baseline.md](memory-audit-baseline.md). Additional:

| ID | Finding | Status |
|----|---------|--------|
| H3 | Deferred delete / GC direct delete | **Open** — documented |
| H5 | Tooltip/packet `new` churn | **Mitigated** — `make_unique` helper |
| M3 | `CWorldSearchHolder` thread-unsafe pool | **Mitigated** — pool size 20→64 |

## E. Script engine

| ID | Finding | Status |
|----|---------|--------|
| E1 | No sandbox | **Open** — documented in architecture.md |
| E2 | No CPU budget per trigger | **Open** |
| E3 | `CScriptTriggerArgs` includes `CObjBase.h` | **Open** |

## F. Build, CI, tests

| ID | Finding | Status |
|----|---------|--------|
| F1 | Main CI compile-only | **Fixed** — `ctest` in `build_linux_x86_64.yml` |
| F2 | Tests mirror logic | **Fixed** — `VendorBuyHelper` shared with `receive.cpp` |
| F3 | No server smoke in CI | **Open** — manual checklist in `utilities/smoke-test.sh` |
| F4 | No static analysis baseline | **Fixed** — `docs/static-analysis-baseline.txt` |

## Tooling

| Tool | Command |
|------|---------|
| ASAN Linux | `./utilities/build-asan-linux.sh` |
| Layering check | `./utilities/check-layering.sh` |
| Unit tests | `cmake -DUNIT_TESTING=ON ... && ctest` |
| Smoke checklist | `./utilities/smoke-test.sh` |
