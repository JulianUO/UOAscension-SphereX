# UltimaLive (Source-X)

Source-X implements the [UltimaLive](https://github.com/SaschaKP/UltimaLive) map streaming protocol used by ClassicUO and the classic client `Igrping.dll` hook.

## What it does

- Streams map terrain (packet `0x40`) and statics (packet `0x3F`) when the client reports block CRC mismatches.
- Sends map definitions and a shard identifier on login / server list.
- Persists live map edits and lumber-harvest state under `UltimaLiveRootPath`.
- Optional **per-character map discovery** — blocks stream only when the character is nearby; undiscovered areas stay blank on the client and on the world map radar.
- Provides staff commands for live map editing (GM+).

## sphere.ini

```ini
UltimaLiveEnabled=1
UltimaLiveShardIdentifier=MyShard
UltimaLiveRootPath=ultimalive/
UltimaLiveStreaming=1
UltimaLiveDiscovery=1
UltimaLiveDiscoveryViewBlocks=3
UltimaLiveDiscoveryRevealOnLogin=1
UltimaLiveMap0=0,7168,4096,5120,4096
```

| Key | Description |
|-----|-------------|
| `UltimaLiveDiscovery` | Enable per-character fog-of-war streaming (requires patched ClassicUO) |
| `UltimaLiveDiscoveryViewBlocks` | Chebyshev block radius around the character to reveal/stream (3 ≈ 24 tiles) |
| `UltimaLiveDiscoveryRevealOnLogin` | Send a discovery snapshot for known blocks near spawn on enter world |

`UltimaLiveMapN` must match your `MapN=` sizes. ClassicUO may need `-maps_layouts` aligned with these dimensions ([Endless Maps](https://github.com/ClassicUO/ClassicUO/wiki/Endless-Maps)).

## Conflicts

- **UseMapDiffs**: UltimaLive clients ignore `0xBF.0x18` mapdiff enablement. Prefer `UseMapDiffs=0` when UltimaLive is enabled.

## Staff commands

Prefix with your command character (default `.`). Requires GM privileges:

- `updateblock`, `queryclienthash`, `getblocknumber`
- `addstatic`, `delstatic`, `setlandid`, `setlandalt`, `inclandalt`
- `refreshclientview`

## Lumberjack vs reference GraphicBasedHarvestSystems

Reference: [praxiiz/UltimaLive GraphicBasedHarvestSystems](https://github.com/praxiiz/UltimaLive/tree/master/ServerSideScripts/GraphicBasedHarvestSystems)

Source-X implements graphic harvest in C++ (`CUltimaLiveHarvest`) with built-in small-tree templates, stumps, fallen phases, and timed regrowth. Extended multi-leaf oak/cedar definitions from the 90KB reference can be added via `trees.ini`.

## Graphic harvest (lumberjack)

When `UltimaLiveHarvest=1`, lumberjacking uses graphic-based harvest aligned with UltimaLive reference behavior:

- Registry of tree trunk graphics (built-in + optional `ultimalive/LumberHarvest/trees.ini`)
- On successful chop: mature tree → fallen logs + stump (`0xE57`), clients receive block updates
- Further chops on fallen pieces remove individual log graphics
- Regrowth on world save when `UltimaLiveHarvestRegrowth=1` and elapsed time ≥ `UltimaLiveHarvestRegrowthMinutes` (default **1440** = 24 hours)
- Persistence: `TreeLocations.{mapId}` (felled locations + regrowth timestamps); legacy `harvest.lumber` is imported on load

| Key | Default | Description |
|-----|---------|-------------|
| `UltimaLiveHarvest` | 1 | Enable graphic harvest (0 = legacy single-static removal) |
| `UltimaLiveHarvestRegrowth` | 1 | Regrow trees on world save |
| `UltimaLiveHarvestRegrowthMinutes` | 1440 | Minutes before a felled tree regrows (1440 = 24h) |

Set `UltimaLiveHarvest=0` to restore the minimal lumberjack shim (remove static on final chop only).

### Sphere setup still required

- Scripts loaded (`ScpFiles=scripts/`)
- Tree statics as `IT_TREE` in item definitions
- Region resources and area regions for `CheckNaturalResource`

## Map discovery protocol

| Packet | Cmd | Purpose |
|--------|-----|---------|
| `0x3F` | `0x02` | Login complete — byte 43 = discovery enabled (1/0) |
| `0x3F` | `0x04` | Discovery snapshot on login (map + block list) |
| `0x3F` | `0x05` | Incremental blocks revealed after movement |

Discovery state is stored per character under `ultimalive/discovery/<char_uid>.bin`.

## Integration test checklist

1. Set `UltimaLiveEnabled=1`, `UltimaLiveDiscovery=1`, `UseMapDiffs=0` in live `sphere.ini`; use patched ClassicUO from `.tmp-classicuo`.
2. New character login — nearby terrain visible; distant areas void/black on world map.
3. Walk into new area — blocks stream in as they enter view radius (`UltimaLiveDiscoveryViewBlocks`).
4. Log out and back in — explored areas restored via `0x3F 0x04` snapshot; unexplored still black.
5. Second character on same account — independent fog (separate `discovery/<uid>.bin` files).
6. Lumberjack / GM `delstatic` — block updates still push to nearby discovered clients.
7. Run unit tests: `t_ultimalive_discovery` (`[ultimalive][discovery]`).

## License note

Protocol handling is based on the MIT-licensed UltimaLive reference implementation by SaschaKP and contributors.
