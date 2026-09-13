# Internal JSON REST API Server

**SphereServer X** (`Source-X`) features an embedded, lightweight C++ HTTP REST API server (`CInternalApiServer.cpp`) running on the game shard. It enables secure inter-process communication between the external Python login server, web management dashboards, and server monitoring agents.

---

## 1. Engine Configuration (`sphere.ini`)

To enable the REST API server on a game shard:

```ini
InternalApiPort=2590
LoginSharedSecret=YourSuperSecretKeyHere
ShardDisplayName=Felucca Shard
```

- `InternalApiPort`: Port number for listening to incoming HTTP REST requests (default `2590`).
- `LoginSharedSecret`: Secret authentication token passed via HTTP `Authorization: Bearer <Secret>` header.

---

## 2. API Endpoints

### 2.1 Session Registration (`POST /internal/v1/sessions`)

Registers an ephemeral authentication token (`AuthID`) created by the login server when a player selects a shard.

#### Request

- **HTTP Method**: `POST`
- **Path**: `/internal/v1/sessions`
- **Headers**:
  - `Authorization: Bearer <LoginSharedSecret>`
  - `Content-Type: application/json`

```json
{
  "account": "player_account_name",
  "auth_id": 2847591023,
  "client_version": 0,
  "reported_version": 7000000,
  "ttl_seconds": 30
}
```

#### Response

- **Status Code**: `200 OK`
```json
{
  "status": "ok",
  "auth_id": 2847591023,
  "account": "player_account_name"
}
```

---

### 2.2 Shard Status & Player Load (`GET /internal/v1/status`)

Polled periodically by external login servers or monitoring dashboards to query current online player counts and shard load metrics.

#### Request

- **HTTP Method**: `GET`
- **Path**: `/internal/v1/status`

#### Response

- **Status Code**: `200 OK`
```json
{
  "status": "online",
  "shard_name": "Felucca Shard",
  "clients_online": 42,
  "clients_max": 500,
  "load_percent": 8
}
```

---

## 3. Session Lifecycle & Token Validation

1. **Session Insertion**: When `POST /internal/v1/sessions` is invoked, `CInternalApiServer` delegates session storage to `CSessionRegistry`.
2. **TTL Expiration**: AuthID session tokens expire automatically after `ttl_seconds` (default 30 seconds) if the player fails to establish a game socket connection.
3. **World Handshake Verification**: When the player's client connects to the game shard port and sends `0x91 CharListReq (AuthID)`, `CClient::Event_Single()` queries `CSessionRegistry::VerifySession()`. If valid, the character selection list is returned and the account is logged in seamlessly.

---

## 4. Related Source Files

- `src/network/CInternalApiServer.cpp` / `.h` — HTTP server request parser & thread
- `src/game/clients/CSessionRegistry.cpp` / `.h` — In-memory session token store with mutex synchronization
- `src/game/clients/CClientLog.cpp` — AuthID token check on connection
