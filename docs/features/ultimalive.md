# UltimaLive Dynamic World Engine & Graphic Harvest

**SphereServer X** (`Source-X`) incorporates full native C++ support for the **[UltimaLive](https://github.com/SaschaKP/UltimaLive)** dynamic map streaming protocol, fog-of-war map discovery tracking, sector overlay management, and a graphic-based harvesting engine for trees and mining veins.

---

## 1. Overview & Capabilities

- **Real-Time Map Streaming**: Streams map terrain blocks (`0x40`) and statics (`0x3F`) live to connected clients (ClassicUO or classic client with `Igrping.dll` / `UltimaLive.dll`) when CRC block mismatches are detected.
- **Fog-of-War Discovery Protocol (`CUltimaLiveDiscovery`)**: Tracks explored map blocks per character. Unexplored tiles remain void/black on the client radar and game screen until the character walks near them.
- **C++ Graphic Harvesting Engine (`CUltimaLiveHarvest`)**:
  - **Lumberjacking (`CUltimaLiveLumber`)**: Dynamic tree chop-down sequence. Mature tree statics transform into fallen log statics and a stump graphic (`0xE57`). Regrowth timer restores the tree automatically after configured interval.
  - **Mining (`CUltimaLiveMining`)**: Dynamic ore vein depletion and tile graphic transitions.
- **Sector Overlays (`CUltimaLiveOverlay`)**: In-memory sector overlay system allowing non-destructive, temporary or instanced map modifications.
- **Staff Commands**: GM tools for editing land height, land tiles, statics, and triggering client view refreshes in real-time.

---

## 2. Configuration Options (`sphere.ini`)

```ini
// UltimaLive Core Streaming
UltimaLiveEnabled=1
UltimaLiveShardIdentifier=MyShard
UltimaLiveRootPath=ultimalive/
UltimaLiveStreaming=1

// Fog-of-War Map Discovery
UltimaLiveDiscovery=1
UltimaLiveDiscoveryViewBlocks=3
UltimaLiveDiscoveryRevealOnLogin=1

// Dynamic Graphic Harvesting
UltimaLiveHarvest=1
UltimaLiveHarvestRegrowth=1
UltimaLiveHarvestRegrowthMinutes=1440

// Map Dimensions Alignment
UltimaLiveMap0=0,7168,4096,5120,4096
UltimaLiveMap1=1,7168,4096,5120,4096
```

### Option Parameters

| Parameter | Default | Description |
|-----------|---------|-------------|
| `UltimaLiveEnabled` | `1` | Master toggle for UltimaLive protocol and map streaming handlers. |
| `UltimaLiveShardIdentifier` | `MyShard` | Unique string identifier passed to clients during handshake. |
| `UltimaLiveRootPath` | `ultimalive/` | Folder path relative to server root for persisting modified map blocks and discovery files. |
| `UltimaLiveDiscovery` | `1` | Enables per-character fog-of-war map streaming (requires patched ClassicUO client). |
| `UltimaLiveDiscoveryViewBlocks` | `3` | Chebyshev block radius around character to reveal and stream (`3` blocks ≈ 24 tiles). |
| `UltimaLiveDiscoveryRevealOnLogin` | `1` | Sends a discovery snapshot packet (`0x3F 0x04`) upon character entering the world. |
| `UltimaLiveHarvest` | `1` | Enables graphic-based harvesting (0 = legacy single-static removal). |
| `UltimaLiveHarvestRegrowth` | `1` | Enables tree/vein regrowth on world saves. |
| `UltimaLiveHarvestRegrowthMinutes` | `1440` | Duration in minutes before a felled tree or depleted vein regrows (1440 = 24h). |

> [!NOTE]
> Set `UseMapDiffs=0` in `sphere.ini` when using UltimaLive, as clients handle diffs via the UltimaLive protocol stream (`0x3F`/`0x40`).

---

## 3. Graphic Harvesting Engine Details

### Tree Chop Down & Regrowth (`CUltimaLiveLumber`)

When `UltimaLiveHarvest=1`:
1. **Tree Identification**: Recognizes tree trunk statics from internal C++ definitions and `ultimalive/LumberHarvest/trees.ini`.
2. **Chop Phase**: Successful lumberjack attempts replace the mature tree static with a stump (`0xE57`) and place fallen log graphic statics on adjacent tiles.
3. **Log Gathering**: Further harvesting chops away the fallen log graphics until only the stump remains.
4. **Regrowth**: On server world save, trees whose elapsed time exceeds `UltimaLiveHarvestRegrowthMinutes` are automatically restored to full mature trees, streaming the update to nearby players.
5. **Persistence**: Felled tree coordinates and regrowth timestamps are saved under `ultimalive/TreeLocations.<mapId>`.

---

## 4. In-Game Staff Commands (GM+)

Prefix commands with the server command key (default `.`):

| Command | Arguments | Description |
|---------|-----------|-------------|
| `.updateblock` | None | Forces block sync to all clients viewing current character location. |
| `.queryclienthash` | None | Queries client block CRC hash for debugging desyncs. |
| `.getblocknumber` | None | Returns current block index and sub-coordinates. |
| `.addstatic` | `[itemid] [z] [color]` | Adds a permanent static tile to the UltimaLive map file. |
| `.delstatic` | `[itemid]` | Removes a static tile at current targeted location. |
| `.setlandid` | `[id]` | Modifies land terrain tile ID at target location. |
| `.setlandalt` | `[z]` | Sets land altitude Z coordinate at target location. |
| `.inclandalt` | `[delta]` | Adjusts land altitude Z coordinate incrementally. |
| `.refreshclientview` | None | Re-sends terrain block packet to current client. |

---

## 5. Map Discovery Protocol Packets

| Packet | Command | Purpose |
|--------|---------|---------|
| `0x3F` | `0x02` | Login Handshake Ack — Byte 43 flags discovery enablement (`1`/`0`). |
| `0x3F` | `0x04` | Discovery Snapshot — Sends initial array of discovered blocks on enter world. |
| `0x3F` | `0x05` | Incremental Block Reveal — Pushes newly discovered map blocks as player moves. |

Explored block state is stored on disk at `ultimalive/discovery/<char_uid>.bin`.

---

## 6. Related Source Files

- `src/game/ultimalive/CUltimaLive.cpp` / `.h` — Protocol engine & map block manager
- `src/game/ultimalive/CUltimaLiveDiscovery.cpp` / `.h` — Character fog-of-war discovery state
- `src/game/ultimalive/CUltimaLiveHarvest.cpp` / `.h` — Base graphic harvest engine
- `src/game/ultimalive/CUltimaLiveLumber.cpp` / `.h` — Tree chop & regrowth logic
- `src/game/ultimalive/CUltimaLiveMining.cpp` / `.h` — Vein mining depletion logic
- `src/game/ultimalive/CUltimaLiveOverlay.cpp` / `.h` — Sector overlay manager
- `tests/src/t_ultimalive.cpp` & `t_ultimalive_discovery.cpp` — Unit test suite
