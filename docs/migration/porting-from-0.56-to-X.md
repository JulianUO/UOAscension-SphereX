# Porting from SphereServer 0.56 to SphereServer X

This guide documents compatibility-breaking changes, script parser improvements, and keyword refactorings when migrating scripts or server configurations from SphereServer 0.56 / 0.56d to **SphereServer X** (`Source-X`).

---

## 1. 0.56b Series Changes

- **Spells & Skills**: Updated spell and skill flags in `sphere_spells.scp` and `sphere_skills.scp`.
- **Mount System**: Mount handling moved to `sphere_defs.scp`. Unlisted mount IDs cannot be ridden.
- **Multis**: Updated multi definitions from `[ITEMDEF 04xxx]` to `[MULTIDEF 0x...]`.
- **Properties**: `MAPPLANE` renamed to `MAP`, `LOCALLIGHT` renamed to `LIGHT`.
- **Encryption**: Encryption keys defined per client version in `SphereCrypt.ini`.

---

## 2. 0.56c Series Changes

- **Mount Keyword**: `MOUNT` verb called on character with mount NPC UID as argument (`MOUNT <uid>`).
- **Add Dialog**: `.ADD` command opens modern dynamic gump (`d_add`).
- **Attacker System**: Combat engine replaced `MEMORY_WAR_TARG` with the internal `Attacker` system (`ATTACKER.x`).
- **Spellbooks**: Spell offset and counts stored in `TDATA3`/`TDATA4` instead of `MOREZ`/`MOREX`.
- **Spell Layers**: `LAYER=layer_spell_*` required for spell definitions.

---

## 3. 0.56d & SphereServer X Migration

### 3.1 Script Parser Enhancements

- **Short-Circuit (Lazy) Evaluation**: `IF`, `ELIF`, and `ELSEIF` condition blocks perform short-circuit logic:
  ```spherescript
  IF (<LINK.ISVALID> && (<LINK.TAG0.TEST> == 1))
      // Safe: if LINK is invalid, second condition is never evaluated!
  ENDIF
  ```
- **Weighted Range Randomization (`{ }`)**: Evaluation occurs ONLY on the randomly selected element within curly braces `{ }`, rather than pre-evaluating all options.
- **Resource Lookups (`RESDEF` vs `DEF`)**:
  - `DEF` is reserved strictly for `[DEFNAME]` tables.
  - To retrieve resource definition IDs (items/chars/spells), use `RESDEF` or `RESOURCEINDEX` (e.g. `<RESOURCEINDEX s_clumsy>`).
- **Resource Aliases (`[RESDEFNAME]`)**: Use `[RESDEFNAME]` sections to define legacy script aliases.
- **Unset / Invalid Object Error Checks**: Accessing invalid/unset references (such as `<REF1.NAME>` when `REF1` is invalid) returns a diagnostic error. Use `<REF1.ISVALID>` or `<REF1>` check first.
- **Explicit `TRY` Verb**: `TRY` can execute commands on potentially invalid references safely without throwing script errors, but cannot be used for value returning expressions.

### 3.2 Keyword & Property Changes

- **`CAN` Property**: `CAN` is read-only. Modify character capabilities at runtime via `CANMASK`.
- **`CLIENTISENHANCED`**: Replaced legacy `CLIENTISSA` command.
- **Client Tooltips (`ADDCLILOC`)**: Must be called on the item/object in `@ClientTooltip`, `@ItemClientTooltip`, and `@CharClientTooltip` triggers.
- **Speech Hue Overrides**: `SPEECHCOLOROVERRIDE` overrides player speech hues.
- **Stat Max Modifiers**: `ModMaxHits`, `ModMaxMana`, `ModMaxStam` added directly to server engine core.
- **Item Values & Shop Prices**: `PRICE` reserved for player vendor selling. Use `TAG.override.value` for customizing item value when selling to NPC vendors.

### 3.3 Multi & Spawn System Changes

- **Multi Persistence**: `t_multi` and `t_multi_custom` objects saved in `spheremultis.scp`.
- **Addon Multis**: Multi decorations use `t_multi_addon`.

---

## 4. Documentation & Tooling Alignment

For detailed syntax rules, section block definitions, trigger contexts, and VSCode scripter plugin support:
- Refer to the **[SphereWiki-X Repository](../../SphereWiki-X/README.md)**.
