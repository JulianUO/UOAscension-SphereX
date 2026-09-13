# Sphere Server X — UOAscension Edition Configuration Guide (`sphere.ini`)

This document details the configuration of **Sphere Server X — UOAscension Edition** via `sphere.ini`, database connections, client protocol settings, and integration options for external services.

---

## 1. Core Server Parameters

Edit `sphere.ini` in your server root directory or `/etc/sphereserver/sphere.ini`:

```ini
[SPHERE]
// Acknowledge usage of nightly pre-release build (REQUIRED to start)
AGREE=1

// Shard Identity
ServName=SphereServer X Shard
ServIP=127.0.0.1
ServPort=2593
AdminEmail=admin@myshard.com

// Client Account Creation Mode
// 0=Closed, 2=Free (auto-create on login), 3=GuestAuto
AccApp=2
```

---

## 2. Client Compatibility & Protocol Flags

```ini
// Supported Client Version (e.g. 7.0.2.0 = 7000000)
ClientVersion=7.0.2.0

// Encryption requirement (0 = auto-detect / no-crypt allowed, 1 = require crypt)
UseCrypt=0

// Client Feature Flags (Expansion features enabled)
// 0x10000 = T2A, 0x20000 = LBR, 0x40000 = AOS, 0x80000 = SE, 0x100000 = ML, 0x200000 = SA
FeatureT2A=01
FeatureLBR=01
FeatureAOS=01
FeatureSE=01
FeatureML=01
FeatureSA=01

// Engine Option Flags
// OF_FileCommands (0x08), OF_NoResetSpeed (0x200), OF_AdvancedSpellParalyze (0x400)
OptionFlags=08 | 0200
```

---

## 3. Map & Client Assets (`MulFiles`) Setup

SphereServer requires access to Ultima Online client map (`.mul` or `.uop`) assets.

```ini
// Option A: Explicit path to UO client directory
MulFiles=C:/Program Files/EA Games/Ultima Online/

// Option B: Local relative folder (e.g. mul/ directory in server root)
MulFiles=mul/

// Map Boundaries Configuration (Felucca = Map0, Trammel = Map1)
Map0=7168,4096,-1,0,0
Map1=7168,4096,-1,1,1
// Comment out unused maps (Map2..Map5) if not in use to conserve memory
//Map2=2304,1600,-1,2,2
```

> [!NOTE]
> On Linux systems, filenames in the `MulFiles` directory must be strictly **lowercase** (`map0.mul`, `statics0.mul`, `tiledata.mul`).

---

## 4. Database Connection Settings

SphereServer X supports MariaDB/MySQL for player account storage and dynamic script queries (`DATABASE.QUERY`).

```ini
// MariaDB / MySQL Configuration
MySQLHost=127.0.0.1
MySQLUser=sphereserver
MySQLPassword=secret_password
MySQLDB=sphereserver_db
MySQLPort=3306

// SQLite Local Storage Configuration (if using SQLite)
SQLiteDB=save/sphereworld.db
```

---

## 5. Subsystem Configurations

### 5.1 UltimaLive Dynamic Map Engine
```ini
UltimaLiveEnabled=1
UltimaLiveShardIdentifier=MyShard
UltimaLiveRootPath=ultimalive/
UltimaLiveDiscovery=1
UltimaLiveHarvest=1
UltimaLiveHarvestRegrowthMinutes=1440
UseMapDiffs=0
```
*(For complete details, see [features/ultimalive.md](features/ultimalive.md))*

### 5.2 External Login Server & REST API
```ini
UseAuthID=1
UseExternalLogin=1
InternalApiPort=2590
LoginSharedSecret=YourSuperSecretKeyHere
ShardDisplayName=Felucca Shard
```
*(For complete details, see [features/login-server.md](features/login-server.md) and [features/internal-api.md](features/internal-api.md))*

### 5.3 Multi-Ship System
```ini
MaxShips=3
```
*(For complete details, see [features/ships-and-housing.md](features/ships-and-housing.md))*

---

## 6. Script Table Registration (`spheretables.scp`)

Ensure script resources are registered in `scripts/spheretables.scp`:

```scp
[SPHERETABLES]
// Core definitions
sphere_defs.scp
sphere_msgs.scp
sphere_spells.scp
sphere_skills.scp

// Map resource tables
maps/map0/map0_areas.scp
maps/map1/map1_areas.scp
```

---

## 7. Scripting & Syntax Reference

For comprehensive scripting rules, section blocks (`[ITEMDEF]`, `[CHARDEF]`, `[FUNCTION]`), triggers (`@Create`, `@DClick`, `@Hit`), and VSCode extension configuration:
- Refer to the **[SphereWiki-X Repository](../../SphereWiki-X/README.md)**.
