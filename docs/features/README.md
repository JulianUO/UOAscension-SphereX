# SphereServer X — Features & Extensions Catalog

This section details every major feature, architecture enhancement, microservice, and system module coded for the **SphereServer X** (`Source-X`) engine fork.

---

## Catalog of Features

| Feature Subsystem | Documentation Guide | Description |
|-------------------|--------------------|-------------|
| **UltimaLive Dynamic World Engine** | [ultimalive.md](ultimalive.md) | Real-time map & static tile streaming, fog-of-war map discovery (`CUltimaLiveDiscovery`), C++ graphic harvesting with dynamic tree chopping & regrowth (`CUltimaLiveLumber`), dynamic vein mining (`CUltimaLiveMining`), and sector overlays. |
| **External Login Server Microservice** | [login-server.md](login-server.md) | Decoupled multi-shard authentication service written in Python, supporting SQLite & MariaDB backends, AuthID token handoffs, and native C++ `sphere_login_crypto` bindings. |
| **Internal JSON REST API Server** | [internal-api.md](internal-api.md) | Embedded C++ HTTP REST server (`CInternalApiServer`) for external web tool integration, server metric collection, session validation, and remote admin control. |
| **Vendor Buy Transaction Engine** | [vendor-buy-helper.md](vendor-buy-helper.md) | High-throughput item purchasing helper (`VendorBuyHelper`) preventing container item limit overflow and optimizing memory allocation during shop transactions. |
| **Extended Ships & Housing Systems** | [ships-and-housing.md](ships-and-housing.md) | Native multi-ship registry per character/account (`MaxShips`, `AddShip`, `DelShip`, `Ships`, `GetShipPos`, `SHIP.x` refs), house design commit triggers (`@HouseDesignCommitItem`, `MAXZ`), and custom multi gump handling. |
| **Movement Prediction & Network Sync** | [movement-and-network.md](movement-and-network.md) | Rubber-banding detection & resolution, walk sequence alignment (0x02 and 0xF0 SA packets), Enhanced Client (EC) support, client encryption 115/116 support, and speech hue overrides (`SpeechColorOverride`). |
| **C++20 Concurrency & Safety Core** | [cpp20-concurrency.md](cpp20-concurrency.md) | C++20 standard migration, multi-threading synchronization (`std::mutex`, `std::unique_ptr`), static thread safety, and layering enforcement (`check-layering.sh`). |

---

## External Scripting & Engine Reference

For complete SphereScript syntax specifications, triggers, function tables, section blocks, and VSCode extension tooling:
- Visit the **[SphereWiki-X Repository](../../SphereWiki-X/README.md)**.
