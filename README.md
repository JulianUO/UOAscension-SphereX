# Sphere Server X — UOAscension Edition

Ultima Online game server engine developed in modern C++20. A specialized engine fork of **SphereServer X** for UOAscension.

<br>

[![GitHub License](https://img.shields.io/github/license/Sphereserver/Source-X?color=blue)](LICENSE)
&nbsp; &nbsp; [![GitHub Repo size](https://img.shields.io/github/repo-size/Sphereserver/Source-X.svg)](https://github.com/Sphereserver/Source-X/)
&nbsp; &nbsp; [![GitHub Stars](https://img.shields.io/github/stars/Sphereserver/Source-X?logo=github)](https://github.com/Sphereserver/Source-X/stargazers)
<br>
[![Coverity Scan Build Status](https://scan.coverity.com/projects/20225/badge.svg)](https://scan.coverity.com/projects/sphereserver-source-x)
&nbsp; &nbsp; [![GitHub Issues](https://img.shields.io/github/issues/Sphereserver/Source-X.svg)](https://github.com/Sphereserver/Source-X/issues)

| Join the SphereServer Community Discord! |
| :---: |
| [![Discord Shield](https://discordapp.com/api/guilds/354358315373035542/widget.png?style=shield)](https://discord.gg/ZrMTXrs) |

---

## 📚 Documentation Hub

Find comprehensive documentation for compiling, configuring, extending, and scripting SphereServer X:

- 📖 **[Installation & Build Guide](docs/installation.md)** — C++20 compiler setup, CMake instructions for Windows/Linux/macOS, build configurations, and Debian systemd packaging.
- ⚙️ **[Configuration Guide](docs/configuration.md)** — Detailed breakdown of `sphere.ini`, database options, MUL asset paths, and client flags.
- 🚀 **[Getting Started Tutorial](docs/Getting-started.md)** — Step-by-step guide for first-time shard setup, world decorators, spawners, and GM admin commands.
- ✨ **[SphereServer X Features Addition](docs/features/README.md)** — Catalog of custom engine features (UltimaLive, External Login Server, REST API, C++20 Concurrency, Ships & Housing, Movement Fixes).
- 🏛️ **[Engine Architecture Blueprint](docs/architecture/architecture.md)** — Core memory models, threading loops, global singletons, and static analysis baselines.
- 🔄 **[Migration & Porting Guide](docs/migration/porting-from-0.56-to-X.md)** — Upgrading scripts and server configurations from 0.55/0.56 to SphereServer X.
- 🌐 **[SphereWiki-X Scripting Repository](../SphereWiki-X/README.md)** — Comprehensive generic wiki for SphereScript section blocks, triggers, function tables, flags, and VSCode extension tooling.
- 🗺️ **[Complete Master Index](docs/INDEX.md)** — Full map of all documentation files in `docs/`.

---

## ✨ What makes UOAscension Edition different?

**Sphere Server X — UOAscension Edition** is a high-performance fork of SphereServer X (0.56d baseline) modernized to C++20, featuring major architectural and custom gameplay enhancements:

- **UltimaLive World Engine**: Native real-time map/static tile streaming, character fog-of-war map discovery (`CUltimaLiveDiscovery`), C++ graphic harvesting with dynamic tree chopping & scheduled regrowth (`CUltimaLiveLumber`), dynamic vein mining (`CUltimaLiveMining`), and sector overlays.
- **External Login Microservice**: Decoupled Python multi-shard login server with SQLite/MariaDB storage, AuthID token authentication, and optional native C++ `sphere_login_crypto` module.
- **Internal REST API Server**: Embedded C++ HTTP REST server (`CInternalApiServer`) for external web tool integration, status polling, and session verification.
- **Vendor Buy Transaction Engine**: Optimized purchasing helper (`VendorBuyHelper`) preventing container item overflows and eliminating memory bloat during shop transactions.
- **Extended Ships & Housing**: Native multi-ship registry per account/character (`MaxShips`, `AddShip`, `DelShip`, `Ships`, `GetShipPos`, `SHIP.x` refs), house design item commit triggers (`@HouseDesignCommitItem`, `MAXZ`), and custom multi gump handling.
- **Movement Prediction & Network Sync**: Rubber-banding elimination, sequence-aligned movement ACK/REJ pipeline (for 0x02 and 0xF0 SA packets), Enhanced Client (EC) speech color overrides (`SpeechColorOverride`), and client encryption 115/116 support.
- **C++20 Concurrency & Memory Safety**: Smart pointer memory management (`std::unique_ptr`), lock hygiene, atomic ref-counts, and strict directory layering integrity (`check-layering.sh`).

---

## ⚡ Quick Start (Building from Source)

Requirements: C++20 compiler (VS 2019+, GCC 8+, Clang 10+), CMake 3.16+, Git.

```bash
# Clone the repository
git clone https://github.com/Sphereserver/Source-X.git
cd Source-X

# Configure with CMake (Linux 64-bit Nightly example)
mkdir build
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/Linux-GNU-x86_64.cmake \
      -G "Unix Makefiles" \
      -DCMAKE_BUILD_TYPE="Nightly" \
      -B ./build -S ./

# Compile
cmake --build ./build --config Nightly
```

For complete compilation parameters, sanitizers (`USE_ASAN`), and platform-specific guides, read the **[Installation Guide](docs/installation.md)**.

---

## 📦 Releases & Scripts

### Server Core Releases

| Branch: Master (Stable Pre-Releases) | Branch: Dev (Active Development) |
|:------------------------------------|:---------------------------------|
| [![GitHub last commit on Master branch](https://img.shields.io/github/last-commit/Sphereserver/Source-X/master.svg)](https://github.com/Sphereserver/Source-X/) &nbsp; [Changelog](Changelog.txt) | [![GitHub last commit on Dev branch](https://img.shields.io/github/last-commit/Sphereserver/Source-X/dev.svg)](https://github.com/Sphereserver/Source-X/tree/dev) &nbsp; [Changelog](Changelog.txt) |
| **Nightly Builds**: <a href="https://github.com/Sphereserver/Source-X/releases">GitHub Nightly Releases</a> | **Sphere Community**: <a href="https://forum.spherecommunity.net/sshare.php?srt=4">Downloads</a> |

### Official ScriptPack

The official script pack is fully compatible with SphereServer X syntax and features while preserving classic systems:
- [Scripts-X GitHub Repository](https://github.com/Sphereserver/Scripts-X)
- [Scripts-X Milestone Releases](https://github.com/Sphereserver/Scripts-X/releases)

---

## 🐧 Debian Daemon Package (`packaging/`)

SphereServer X can be compiled and installed as a native Linux `systemd` service:
- Binary location: `/usr/bin/sphereserver`
- Configuration: `/etc/sphereserver/sphere.ini`
- Server root: `/opt/sphereserver/`
- Logs: `/var/log/sphereserver/`

See [docs/installation.md#debian--ubuntu-package-installation-packaging](docs/installation.md#debian--ubuntu-package-installation-packaging) for build and installation steps (`sudo systemctl start sphereserver`).

---

## 👥 Contributing & Licensing

See [CONTRIBUTING.md](CONTRIBUTING.md) for details on code style, git workflows, and pull requests.

Copyright 2026 SphereServer development team.  
Licensed under the **Apache License, Version 2.0**. See [LICENSE](LICENSE) for details.
