# Vendor Buy Transaction Engine (`VendorBuyHelper`)

**SphereServer X** (`Source-X`) implements an optimized, high-throughput vendor purchase pipeline (`src/network/VendorBuyHelper.cpp`). It resolves legacy issues where purchasing large volumes or stacks of items from NPC vendors could overflow packet buffers, trigger container item limits, or cause client/server item desynchronization.

---

## 1. Problem Addressed

In legacy versions of SphereServer:
- Buying multi-item orders from NPC vendors created numerous intermediate container objects during transaction evaluation.
- Large transactions risks hitting container capacity limits or creating corrupted item stacks in player backpacks.
- Processing bulk buy commands generated high heap allocation overhead and packet fragment desyncs under heavy player load.

---

## 2. Key Architecture Features

1. **Pre-Allocation & Capacity Validation**: Validates backpack weight and item container slot constraints BEFORE mutating items or transferring currency.
2. **Gold & Commodity Check**: Seamlessly checks bank gold, backpack gold, and currency tokens with transactional rollback support on failure.
3. **Stack Consolidation**: Automatically merges purchased stackable items (potions, reagents, arrows, bandages) into existing backpack stacks, minimizing item entity creation in `g_World`.
4. **Memory Allocation Safety**: Utilizes stack-allocated buffers and fast vector lookups, avoiding heavy dynamic memory allocations during high-frequency NPC trading.

---

## 3. Related Source Files & Tests

- `src/network/VendorBuyHelper.cpp` / `.h` — Main transaction helper implementation
- `src/game/clients/CClientTarg.cpp` — Vendor buy packet handler entry point
- `tests/src/t_vendor_buy.cpp` — Unit test suite validating transaction boundaries and stack aggregation
