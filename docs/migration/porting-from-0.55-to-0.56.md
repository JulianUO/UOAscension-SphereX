# Porting from SphereServer 0.55 to 0.56

Historical reference for migrating legacy SphereServer 0.55 scripts and world saves to SphereServer 0.56 / SphereX.

---

## 55r2 Changes

- **FixAction functions**: No longer required by core.
- **Spell Interrupts**: Added `INTERRUPT` keyword to `sphere_spells.scp`.

---

## 55r3 Changes

- **`TRY` vs `TRYP`**: `TRY` executes without privilege or distance/touch checks. Use `TRYP` if touch/distance/plevel checks are needed.
- **`RANGE` Property**: Supports `RANGE=min,max` or `RANGE=max` on `ITEMDEF` and `CHARDEF`.
- **Section Headers**: Renamed `[AREA ...]` to `[AREADEF ...]`.
- **Global Data Persistence**: Introduced `spheredata.scp` under `save/` directory to store global variables, region/sector/room dynamic modifications.

---

## 55r3rc2 Changes

- **Default Messages**: Added `DEFMESSAGE` system (`sphere_msgs.scp`).
- **Spell Flags**: Updated `SPELLFLAG_TARG_*` in `sphere_spells.scp` and `sphere_defs.scp`.
- **Stat Modifier System**: Stat-modifying spells and cursed items migrated from modifying base stats to `MODSTR`/`MODDEX`/`MODINT`.

---

## 55r403 Changes

- **Static Objects Persistence**: Introduced `spherestatics.scp` for `ATTR_STATIC` items to speed up main world saves. Save command: `SERV.SAVESTATICS`.
- **Keyword Standardizations**:
  - `*WEIGHTMAX` → `*MAXWEIGHT`
  - `BRAIN` → `NPC`
  - `KNOWLEDGE` → `SPEECH`
  - `COMMENT` → `TAG.COMMENT`
  - `CLI*VER*` → `CLIENTVERSION`
  - `DEF` → `ARMOR`
  - `LISTEN` → `HEARALL`
  - `CLIENTS` → `ALLCLIENTS`
  - `PARDON` → `FORGIVE`
  - `INVISIBLE` → `INVIS`
  - `INVULNERABLE` → `INVUL`
  - `ADDITEM` / `ADDNPC` → Unified into `ADD`
