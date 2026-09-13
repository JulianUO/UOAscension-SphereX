# Movement rubber-banding — investigation handoff

Handoff document for continuing work on player walk/collision rubber-banding in Source-X (Sphere). Intended for another developer picking up the task.

**Status: proposed fixes (1–3) and high-priority follow-up (walk-state reset on reject) have been applied.** Medium/low-priority structural work remains open.

---

## Problem statement

Players experience **rubber-banding** when walking or running into another player, NPC, or blocking object — especially at higher latency and when approaching from more than one tile away.

Symptoms reported:

- Movement looks clean when walking into a blocker from **one tile away**.
- From **further away** (while running), the client keeps sending walk requests and the character stutters/snaps back repeatedly.
- Work was started around `CClient::Event_Walk()` and `PacketMovementAck` / `PacketMovementRej` handling (e.g. ack sent inside `if (iRet == TRIGRET_RET_TRUE)` after `CheckLocationEffects`).

---

## Key files

| File | Role |
|------|------|
| `src/game/clients/CClientEvent.cpp` | `CClient::Event_Walk()` — main walk pipeline |
| `src/game/chars/CCharAct.cpp` | `CanMoveWalkTo()`, `ShoveCharAtPosition()`, `CheckLocationEffects()` |
| `src/network/receive.cpp` | `PacketMovementReq` (0x02), `PacketMovementReqNew` (0xF0) |
| `src/network/send.cpp` | `PacketMovementAck` (0x22), `PacketMovementRej` (0x21) |
| `src/game/clients/CClientEvent.cpp` | `Event_CheckWalkBuffer()` — run-speed buffer |

Reference emulators for comparison:

- **RunUO / ServUO**: `Server/Mobile.cs` → `Move()`, `CheckMovement()`, `OnMoveOver` / `OnMoveOff`
- **RunUO**: `Server/Network/PacketHandlers.cs` → `MovementReq`
- **ModernUO**: `Projects/Server/Mobiles/Mobile.cs` → `CanMove()`, `Move()`, `CheckShove()`

---

## UO movement protocol (brief)

Client sends **0x02 Request Movement** (or **0xF0** on SA clients) with:

- `direction` (walk/run flag in high bits)
- `sequence` (0–255, increments on each accepted step)

Server replies with:

- **0x22 Walk Ack** — step accepted; client advances sequence
- **0x21 Walk Reject** — step denied; client snaps to server position; sequence resets

If the server accepts a step on its side (`Event_Walk` returns `true`) but does not send an ack, or moves the character then rejects without rollback, the client desyncs and rubber-bands.

---

## Architecture comparison

### RunUO / ServUO / ModernUO

Validation happens **before** position changes:

```
CheckMovement (terrain / Z)
  → OnMoveOff / OnMoveOver (chars + items at old/new tile)
  → Region.CanMove
  → fastwalk / timing checks
  → send 0x22 MovementAck
  → SetLocation (apply move)
```

Player blocking: destination mobile's `OnMoveOver(mover)` calls `CheckShove()`. If shove fails, `Move()` returns `false`, handler sends `MovementRej`, and **the server never moved**.

RunUO packet handler on failure:

```csharp
if (!m.Move(dir)) {
    state.Send(new MovementRej(seq, m));
    state.Sequence = 0;
    m.ClearFastwalkStack();
}
```

### Sphere (after applied fixes)

```
CanMoveWalkTo()        — terrain + ShoveCharAtPosition (chars)
fastwalk / walk-buffer — timing checks (before move)
MoveToChar()           — position applied
CheckLocationEffects() — @STEP, teleports, items; rollback on TRIGRET_RET_FALSE
Set STATF_FLY          — running flag from rawdir
PacketMovementAck      — when iRet != TRIGRET_RET_FALSE
Event_ClearWalkState() — on every MovementRej
```

| Check | ServUO / ModernUO | Sphere (after fixes) |
|-------|-------------------|----------------------|
| Terrain / Z | `CheckMovement()` | `CheckValidMove()` in `CanMoveWalkTo()` |
| Player on tile | `OnMoveOver` → `CheckShove` | `ShoveCharAtPosition()` (+ `@PersonalSpace`, `@charShove`) |
| Items on tile | `item.OnMoveOver()` **before** move | `@ItemStep` / hardcoded types in `CheckLocationEffects()` **after** move |
| Timing / anti-speedhack | **Before** `SetLocation` | **Before** `MoveToChar` |

Sphere's character shove logic is broadly correct for single-tile blocking. The main gaps are **ordering** (when checks run vs when position updates) and **sequence/ack consistency**.

---

## Root causes identified

### 1. Fastwalk / walk-buffer rejected after `MoveToChar` (primary bug)

When running from a distance, high-latency clients send multiple walk packets in flight. Sphere currently:

1. Moves the character on the server (`MoveToChar`)
2. Then runs fastwalk / walk-buffer checks
3. Sends `PacketMovementRej` **without rolling back** position

The client snaps back while the server had already advanced — classic rubber-banding. One tile away often means a single packet, so the bug is less visible.

**ServUO/ModernUO always gate timing before applying the new position.**

### 2. `PacketMovementReqNew` (0xF0) does not advance `m_sequence`

For SA clients that batch multiple steps in one 0xF0 packet, successful steps do not update `net->m_sequence` (only failures reset it to 0). That desyncs client/server sequence over longer runs.

Classic 0x02 handler (`PacketMovementReq::onReceive`) already updates sequence correctly.

### 3. Ack gated on `TRIGRET_RET_TRUE` only

`CheckLocationEffects()` returns:

- `TRIGRET_RET_FALSE` — blocked (handled with rollback + reject)
- `TRIGRET_RET_TRUE` — normal step complete
- `TRIGRET_RET_DEFAULT` — e.g. teleports via `Spell_Teleport`

Currently, ack + `UpdateMove` / `addPlayerSee` run only when `iRet == TRIGRET_RET_TRUE`. Teleport steps can return `true` from `Event_Walk` while **no ack** is sent, yet `receive.cpp` still increments sequence — another desync path.

### 4. Item / region blocking is post-move (design gap)

`@ItemStep` and some hardcoded tile effects run in `CheckLocationEffects()` after `MoveToChar`, with rollback on failure. ServUO checks `OnMoveOver` on items **before** moving. This architectural difference can cause edge-case stutter.

### 5. Fastwalk system is known-fragile (pre-existing)

Comments in `Event_Walk` and `sphere.ini` note that fixed millisecond offsets cause false positives for high-ping players. `Event_CheckWalkBuffer` is documented as more tunable. ModernUO uses accumulative `_nextMovementTime` with drift clamping instead of a single global offset.

---

## Applied fixes

These changes have been implemented in the codebase.

### Fix 1 — `CClient::Event_Walk()` (`src/game/clients/CClientEvent.cpp`)

**Move fastwalk and walk-buffer checks before `MoveToChar`.**

Current order (problematic):

```
CanMoveWalkTo → MoveToChar → CheckLocationEffects → fastwalk/walk-buffer → ack
```

Target order (aligned with ServUO):

```
CanMoveWalkTo → fastwalk/walk-buffer → MoveToChar → CheckLocationEffects → ack
```

This ensures timing rejects happen before the server changes position, eliminating reject-without-rollback rubber-banding when multiple walk packets arrive while running.

**Also:** change the walk-buffer condition from `IsStatFlag(STATF_FLY)` to `(rawdir & DIR_MASK_RUNNING)`, because `STATF_FLY` is only set later in the pipeline (after `MoveToChar`). Using `STATF_FLY` for the buffer check means it reflects the *previous* step's run state, not the current packet.

### Fix 2 — `CClient::Event_Walk()` ack gating

**Send ack + visibility updates when `iRet != TRIGRET_RET_FALSE`**, not only when `iRet == TRIGRET_RET_TRUE`.

This covers normal steps and teleport steps (`TRIGRET_RET_DEFAULT`) so sequence stays aligned when `Event_Walk` returns `true`.

### Fix 3 — `PacketMovementReqNew::onReceive()` (`src/network/receive.cpp`)

After each **successful** step in a batched 0xF0 packet, update `net->m_sequence` the same way as the 0x02 handler:

```cpp
if ( sequence == UINT8_MAX )
    sequence = 0;
net->m_sequence = ++sequence;
```

Previously only failures reset `m_sequence` to 0; successes left it stale.

### Fix 4 — `CClient::Event_ClearWalkState()` (high-priority follow-up)

Added `Event_ClearWalkState()` and call it on every `PacketMovementRej` path in `Event_Walk()`. Resets `m_timeNextEventWalk`, `m_iWalkStepCount`, and `m_timeWalkStep` (mirrors RunUO `ClearFastwalkStack()` on reject).

---

## Further work (remaining)

### High priority

1. **Verify in-game** using the test plan below (walk + run, 1 tile vs 5+ tiles, high latency, into player/NPC/wall).
2. **Confirm 0xF0 path** — test with `FEATURE_SA_MOVEMENT` clients under real batched-movement conditions.

### Medium priority (structural alignment with ServUO)

3. **Split validate vs apply** — refactor toward `CanWalkTo()` (all blocking checks, no position change) then `ApplyWalk()` (ack + move + side effects), mirroring ServUO `CanMove()` + `Move()`.
4. **Pre-move item checks** — evaluate whether `@ItemStep` / blocking items can be checked before `MoveToChar` to avoid move-then-rollback.
5. **Ping-aware timing** — replace or supplement fixed `m_timeNextEventWalk` offset with per-client timing (see ModernUO `NetState._nextMovementTime`).

### Low priority

6. Review `ShoveCharAtPosition()` vs ServUO `CheckShove()` for stamina / hidden / GM edge cases.
7. Document `sphere.ini` tuning for `WalkBuffer`, `FastWalkPrevention`, and `FEATURE_SA_MOVEMENT`.

---

## Test plan

| # | Scenario | Expected |
|---|----------|----------|
| 1 | Walk 1 tile into stationary player | Clean stop, one reject, no bounce loop |
| 2 | Run 5+ tiles into stationary player (simulate latency if possible) | Clean stop at blocker, no repeated rubber-band |
| 3 | Walk into NPC (non-shoveable config) | Same as player block |
| 4 | Walk into wall / impassable terrain | Reject, no server-side advance |
| 5 | `FastWalkPrevention` on, high ping | No false rejects on legitimate running |
| 6 | `WalkBuffer` on, running on foot | No stutter except at true speedhack rates |
| 7 | Step on moongate / telepad | Teleport works; client stays in sync |
| 8 | SA client with `FEATURE_SA_MOVEMENT`, batched 0xF0 | Sequence stays aligned over multi-step runs |
| 9 | Direction-only change (no move) | Ack sent, no position change |

---

## Prompt for the next developer

Copy everything below this line into a new chat or ticket.

---

**Task: Fix movement rubber-banding in Source-X (Sphere server)**

**Context**

Source-X is a Sphere UO server emulator (C++). Players rubber-band when walking/running into other players, NPCs, or blocking objects — especially from more than one tile away and at higher latency.

An investigation compared Sphere's walk pipeline to RunUO/ServUO/ModernUO. Full analysis is in:

`docs/movement-rubberbanding-handoff.md`

**Fixes 1–4 have been applied.** Validate in-game using the test plan in this doc. Remaining work is structural (see “Further work” section).

**Already implemented**

1. `Event_Walk()` — fastwalk/walk-buffer before `MoveToChar`; walk-buffer uses `(rawdir & DIR_MASK_RUNNING)`; ack on `iRet != TRIGRET_RET_FALSE`.
2. `PacketMovementReqNew` — `m_sequence` incremented per successful batched step.
3. `Event_ClearWalkState()` — called on every `MovementRej` in `Event_Walk()`.

**Remaining work**

4. Optionally refactor toward ServUO's model: all blocking checks (terrain, chars, items, region, timing) **before** position change; ack then apply move.
5. Evaluate moving `@ItemStep` / item blocking pre-`MoveToChar` to avoid move-then-rollback.
6. Evaluate ping-aware movement timing instead of fixed `m_timeNextEventWalk` offsets (ModernUO `NetState._nextMovementTime`).

**Key files**

- `src/game/clients/CClientEvent.cpp` — `Event_Walk`, `Event_CheckWalkBuffer`
- `src/game/chars/CCharAct.cpp` — `CanMoveWalkTo`, `ShoveCharAtPosition`, `CheckLocationEffects`
- `src/network/receive.cpp` — `PacketMovementReq`, `PacketMovementReqNew`
- `src/network/send.cpp` — `PacketMovementAck`, `PacketMovementRej`

**Reference implementations**

- ServUO: `Server/Mobile.cs` (`Move`, `CheckMovement`, `OnMoveOver`, `CheckShove`)
- RunUO: `Server/Network/PacketHandlers.cs` (`MovementReq`)
- ModernUO: `Projects/Server/Mobiles/Mobile.cs` (`CanMove`, `Move`)

**Constraints**

- Minimize scope; match existing Sphere naming and patterns.
- Do not change unrelated systems.
- Only add tests if they cover real walk/block/sequence behavior.

**Success criteria**

- Running from 5+ tiles into a stationary player stops cleanly with no bounce loop.
- No server-side position advance when a step is rejected (except intentional rollback paths).
- Client walk sequence stays aligned for both 0x02 and 0xF0 packets.
- No regressions on teleports, moongates, or direction-only turns.

---

## References

- [UO protocol: Request Movement (0x02)](https://www.mirror.ashkantra.de/joinuo/Documents/Packet%20Guides/2007-03-20%20kairpacketguide/packet02.htm)
- RunUO `MovementReq`: https://github.com/runuo/runuo/blob/master/Server/Network/PacketHandlers.cs
- ServUO `Mobile.Move`: https://github.com/ServUO/ServUO/blob/master/Server/Mobile.cs
- ModernUO `Mobile.Move`: https://github.com/modernuo/ModernUO/blob/main/Projects/Server/Mobiles/Mobile.cs
