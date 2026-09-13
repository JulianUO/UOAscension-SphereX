# SphereServer X Documentation Index

Welcome to the **SphereServer X** (`Source-X`) documentation repository. Use this index to navigate server guides, architecture blueprints, feature specifications, and historical migration references.

---

## 📖 Getting Started & Core Guides

- **[Installation Guide](installation.md)**: C++20 compiler requirements, CMake compilation instructions for Windows, Linux, and macOS, and Debian systemd service packaging.
- **[Configuration Guide](configuration.md)**: Master `sphere.ini` breakdown, database settings, MUL asset loading, and protocol options.
- **[Getting Started Tutorial](Getting-started.md)**: Step-by-step guide for setting up your first SphereServer X shard, populating the world with spawner tools, and promoting GM admins.
- **[Contributor Guidelines](contributing.md)**: Guidelines for contributing code, C++ formatting standards (`.clang-format`), and GitHub pull requests.

---

## ✨ Features & Fork Extensions (`docs/features/`)

Master index: **[Features Catalog](features/README.md)**

1. **[UltimaLive Dynamic World Engine](features/ultimalive.md)** — Real-time map & static tile streaming, fog-of-war map discovery (`CUltimaLiveDiscovery`), dynamic tree chopping & regrowth (`CUltimaLiveLumber`), dynamic vein mining (`CUltimaLiveMining`), and sector overlays (`CUltimaLiveOverlay`).
2. **[External Login Server Microservice](features/login-server.md)** — Python-based multi-shard authentication service, AuthID handoff protocol, SQLite/MariaDB integration, and native C++ `sphere_login_crypto` module.
3. **[Internal REST API Server](features/internal-api.md)** — Embedded C++ HTTP REST server (`CInternalApiServer`) for session authorization (`POST /internal/v1/sessions`) and status monitoring (`GET /internal/v1/status`).
4. **[Vendor Buy Transaction Engine](features/vendor-buy-helper.md)** — High-throughput shop helper (`VendorBuyHelper`) preventing container item overflows and optimizing memory allocation.
5. **[Extended Ships & Housing Systems](features/ships-and-housing.md)** — Multi-ship registry per player (`MaxShips`, `AddShip`, `DelShip`, `Ships`, `GetShipPos`, `SHIP.x` refs), house design commit triggers (`@HouseDesignCommitItem`, `MAXZ`), and custom multi gump handling.
6. **[Movement Prediction & Network Sync](features/movement-and-network.md)** — Rubber-banding fixes, sequence alignment (0x02 and 0xF0 packets), Enhanced Client (EC) support, client encryption 115/116, and speech hue overrides (`SpeechColorOverride`).
7. **[C++20 Concurrency & Safety Core](features/cpp20-concurrency.md)** — C++20 standard refactoring, smart pointer memory management (`std::unique_ptr`), lock hygiene, and layering enforcement (`check-layering.sh`).

---

## 🏛️ Architecture & Engine Audits (`docs/architecture/`)

- **[Server Architecture Blueprint](architecture/architecture.md)**: Memory ownership, main loop dispatching, network thread pool, global singletons, and directory layering rules.
- **[Foundation Audit Baseline](architecture/foundation-audit-baseline.md)**: Core engine stability baseline, exception safety checks, and reference counting audit.
- **[Memory Audit Baseline](architecture/memory-audit-baseline.md)**: Memory allocation profiling and leak prevention baselines.
- **[Static Analysis Baseline](architecture/static-analysis-baseline.md)**: Static analysis setup (CppCheck, clang-tidy, CodeQL) and layering audit script.

---

## 🔄 Migration & Porting (`docs/migration/`)

- **[Porting from 0.56 to SphereServer X](migration/porting-from-0.56-to-X.md)**: Script parser changes (short-circuit `IF` evaluation, curly brace `{ }` range parsing, `RESDEF` vs `DEF`), keyword renamings, and multi format changes.
- **[Porting from 0.55 to 0.56](migration/porting-from-0.55-to-0.56.md)**: Historical script changes between 0.55 and 0.56 releases.

---

## 📜 Historical Changelogs (`docs/changelogs/`)

- **[Changelog 0.56c Pre-Release](changelogs/Changelog-56c-PreRelease.txt)**
- **[Changelog 0.56b Pre-Release](changelogs/Changelog-56b-PreRelease.txt)**
- **[Changelog 0.56a Pre-Release](changelogs/Changelog-56a-PreRelease.txt)**
- **[Changelog 0.55 Series](changelogs/Changelog-55-SERIES.txt)**
- **[Changelog 0.51 - 0.54 Series](changelogs/Changelog-51-54-SERIES.txt)**

---

## 📚 Legacy References (`docs/legacy/`)

- **[Old Scripting Manual](legacy/scripting-manual-old.md)**: Legacy SphereScript syntax documentation.
- **[Client Sound Effect IDs](legacy/sounds.md)**: Hexadecimal and decimal table of Ultima Online sound effect IDs.
- **[Typecast Reference](legacy/typecast.md)**: Historical variable typecasting reference.

---

## 🌐 Generic SphereScript Wiki Reference

For comprehensive SphereScript definitions, section block specifications (`[ITEMDEF]`, `[CHARDEF]`, `[FUNCTION]`), trigger specifications, and VSCode extension tooling:
- Consult the **[SphereWiki-X Repository](../../SphereWiki-X/README.md)**.
