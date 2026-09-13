# Movement Prediction, Network Sync & Extended Client Support

This document details the movement pipeline, rubber-banding prevention mechanisms, client handshake enhancements, and modern client protocol support implemented in **SphereServer X** (`Source-X`).

---

## 1. Movement Pipeline & Rubber-Banding Prevention

### 1.1 The Movement Protocol

The Ultima Online client requests movement via **`0x02 Request Movement`** (Classic Client) or **`0xF0 Stygian Abyss Movement`** (SA / Enhanced Client):
- Each packet includes `direction` (with run flag bit `0x80`) and a sequence number (`0`–`255`).
- The server responds with:
  - **`0x22 Movement Ack`**: Step accepted; client advances sequence.
  - **`0x21 Movement Rej`**: Step rejected; client snaps back to server position and sequence resets to 0.

### 1.2 Fixed Pipeline Ordering

In legacy implementations, fastwalk and walk-buffer timing checks ran AFTER `MoveToChar` had already updated the character position in `g_World`. If timing checks subsequently rejected the movement, `PacketMovementRej` was sent without rolling back position, causing severe client rubber-banding under higher latency.

**SphereServer X** enforces strict pipeline ordering:

```
CanMoveWalkTo (terrain / Z / collision checks)
  ↓
Fastwalk & Walk-Buffer Timing Validation (BEFORE position update)
  ↓
MoveToChar (apply position to server world state)
  ↓
CheckLocationEffects (@STEP / item triggers / teleports)
  ↓
PacketMovementAck & Sequence Advancement
```

### 1.3 Key Network Sync Improvements

1. **Batched SA Movement (`0xF0`) Sequence Tracking**: Correctly increments `net->m_sequence` for every successful step in batched packets (`src/network/receive.cpp`).
2. **Ack Gating Fix**: Sends Movement Ack (`0x22`) whenever `CheckLocationEffects()` returns `!= TRIGRET_RET_FALSE`, ensuring teleports (`TRIGRET_RET_DEFAULT`) maintain sequence synchronization.
3. **Walk-State Reset on Reject**: Resets movement timers and step counters via `CClient::Event_ClearWalkState()` on every Movement Reject path.

---

## 2. Extended Client Support & Speech Hues

### 2.1 Encryption 115 & 116 Support

Added native engine support for client encryptions version 115 and 116, ensuring seamless compatibility with modern Enhanced Clients and updated Classic Client builds.

### 2.2 Enhanced Client (EC) Speech Color Overrides

Enhanced Clients display default NPC speech in yellow hue unless forced. **SphereServer X** automatically uses Unicode speech mode for EC clients when `SAY_DEF_UNICODE=1`, enabling correct NPC speech colors.

#### `SpeechColorOverride` Property
- **Target**: Player characters (`CCharPlayer`).
- **Access**: Read / Write.
- **Description**: Overrides client speech hue sent to other players (`SpeechColorOverride = 52` restores default yellow hue).

---

## 3. Related Source Files

- `src/game/clients/CClientEvent.cpp` — `Event_Walk()`, `Event_CheckWalkBuffer()`, `Event_ClearWalkState()`
- `src/game/chars/CCharAct.cpp` — `CanMoveWalkTo()`, `ShoveCharAtPosition()`, `CheckLocationEffects()`
- `src/network/receive.cpp` — `PacketMovementReq` (0x02) and `PacketMovementReqNew` (0xF0) handlers
- `src/network/send.cpp` — `PacketMovementAck` (0x22) and `PacketMovementRej` (0x21)
