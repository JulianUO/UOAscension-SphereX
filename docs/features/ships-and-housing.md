# Expanded Ships & Housing Systems

**SphereServer X** (`Source-X`) expands native engine support for multi-ship ownership, player ship registries, dynamic house design commit triggers, and custom multi gump handling.

---

## 1. Multi-Ship Registry System

In legacy Sphere, player ship tracking relied primarily on character memory items (`MEMORY_GUARD` / `0x100`). **SphereServer X** introduces native multi-ship registration attached directly to accounts and characters.

### 1.1 `sphere.ini` Settings

```ini
MaxShips=3
```
- `MaxShips`: Sets default maximum allowed ships owned simultaneously by a single character or account.

### 1.2 Script Properties & Methods

| Property / Verb | Access | Description |
|-----------------|--------|-------------|
| `MaxShips` | Read / Write | Gets or sets ship limit for the target account or character (`<SRC.MAXSHIPS>`). |
| `Ships` | Read-Only | Returns current count of registered ships owned by the player (`<SRC.SHIPS>`). |
| `AddShip <uid>` | Write-Only | Registers the given ship UID to player ship registry. If ship count exceeds `MaxShips`, returns false or redeeds the ship. |
| `DelShip <uid>` | Write-Only | Removes target ship UID from registry (`-1` clears whole list). |
| `GetShipPos <uid>` | Read-Only | Returns 0-based index of ship UID in list (`-1` if not found). |
| `SHIP.<n>` | Reference | Object reference to the Nth ship in player list (e.g. `<SRC.SHIP.1.NAME>`, `<SRC.SHIP.1.P>`). |

---

## 2. House Design & Customization Triggers

For custom house design mode, **SphereServer X** provides explicit triggers to inspect, filter, or alter items placed during dynamic house customization.

### 2.1 `@HouseDesignCommitItem` Trigger

Fires for every individual item placed or committed during house design commit phase.

- **Trigger Context**:
  - `this`: The multi item (`CItemMultiCustom`).
  - `SRC`: The character committing the design.
  - `local.id`: Item graphic ID.
  - `local.p.x`, `local.p.y`, `local.p.z`: Relative coordinates within the house multi.
  - `local.visible`: `1` if visible, `0` for structural/invisible elements (doors, signs).
- **Return Values**:
  - `RETURN 0`: Allow item commit (updates `local.MaxZ` baseline).
  - `RETURN 1`: Block/reject this specific item from being committed into the house layout.

### 2.2 `@HouseDesignCommit` Trigger

Fires once when the entire custom house design commit process starts.
- `local.MAXZ`: Contains the maximum Z height achieved by committed design items.

---

## 3. Related Source Files

- `src/game/items/CItemMulti.cpp` & `CItemMultiCustom.cpp` — Multi object handling and custom house commitment engine
- `src/game/clients/CAccount.cpp` & `CCharPlayer.cpp` — Ship registry storage and limits
- `src/tables/CChar_functions.tbl` — Ship verb definitions
