#!/usr/bin/env python3
"""Source-X external login server (multi-shard)."""

from __future__ import annotations

import argparse
import asyncio
import json
import logging
import struct
import zlib
from dataclasses import dataclass
from pathlib import Path
from typing import Optional, Tuple

LOG = logging.getLogger("login_server")

LOGIN_PACKET_SIZE = 62
PLAY_SERVER_PACKET_SIZE = 3

from accounts import AccountStore
from session_client import register_session
from shard_registry import ShardRegistry
import sphere_login_crypto as crypto


LOGIN_ERROR_SUCCESS = 0
LOGIN_ERROR_BAD_PASS = 1
LOGIN_ERROR_BLOCKED = 2
LOGIN_ERROR_OTHER = 4


@dataclass
class LoginConfig:
    listen_host: str
    listen_port: int
    shared_secret: str
    md5_passwords: bool
    account_store: AccountStore
    shard_registry: ShardRegistry


class LoginClientState:
    def __init__(self) -> None:
        self.seed: int = 0
        self.reported_version: int = 0
        self.account: str = ""
        self.password: str = ""
        self.authenticated: bool = False
        self.use_login_crypt: bool = True


async def read_seed(reader: asyncio.StreamReader) -> Tuple[int, int]:
    first = await reader.readexactly(1)
    if first[0] == 0xEF:
        rest = await reader.readexactly(20)
        data = first + rest
        # ClassicUO sends the seed block in big-endian (local IP + client version).
        seed = struct.unpack_from(">I", data, 1)[0]
        major, minor, rev, patch = struct.unpack_from(">IIII", data, 5)
        reported = major * 1_000_000 + minor * 10_000 + rev * 100 + patch
        return seed, reported
    rest = await reader.readexactly(3)
    seed = struct.unpack(">I", first + rest)[0]
    return seed, 0


def parse_servers_req(packet: bytes) -> Optional[Tuple[str, str]]:
    if len(packet) < 62 or packet[0] != 0x80:
        return None
    account = packet[1:31].split(b"\x00", 1)[0].decode("ascii", errors="ignore").strip()
    password = packet[31:61].split(b"\x00", 1)[0].decode("ascii", errors="ignore").strip()
    return account, password


def make_auth_id(account: str, shard_name: str) -> int:
    payload = f"{account}:{shard_name}".encode("utf-8")
    return zlib.crc32(payload) & 0xFFFFFFFF


def parse_play_server(packet: bytes) -> Optional[int]:
    if len(packet) < 3 or packet[0] != 0xA0:
        return None
    # ClassicUO sends 0xA0, 0x00, <index>. Official clients use a uint16 LE index.
    if packet[1] == 0:
        return packet[2]
    return struct.unpack_from("<H", packet, 1)[0]


async def read_login_packet(reader: asyncio.StreamReader, seed: int, size: int) -> Tuple[bytes, bool]:
    data = await reader.readexactly(size)
    return crypto.decode_login_packet(seed, data)


async def send_login_response(
    writer: asyncio.StreamWriter,
    seed: int,
    payload: bytes,
    use_login_crypt: bool,
) -> None:
    writer.write(crypto.encode_login_packet(seed, payload, use_login_crypt))
    await writer.drain()


async def send_login_error(
    writer: asyncio.StreamWriter,
    seed: int,
    code: int,
    reason: str,
    peer,
    use_login_crypt: bool,
) -> None:
    LOG.info("%s login error %d (%s)", peer, code, reason)
    await send_login_response(writer, seed, crypto.build_login_error(code), use_login_crypt)


def build_server_list_packet(shards, host_to_ip_be) -> bytes:
    entries = []
    for idx, shard in enumerate(shards, start=1):
        ip_be = host_to_ip_be(shard.host)
        entries.append(
            crypto.build_server_entry(idx, shard.name, shard.percent_full, shard.timezone, ip_be)
        )
    return crypto.build_server_list_packet(entries)


async def handle_client(reader: asyncio.StreamReader, writer: asyncio.StreamWriter, cfg: LoginConfig) -> None:
    state = LoginClientState()
    peer = writer.get_extra_info("peername")
    try:
        state.seed, state.reported_version = await read_seed(reader)
        if state.seed == 0:
            LOG.warning("%s sent zero seed, closing", peer)
            return

        LOG.info(
            "%s connected seed=0x%08X client_version=%s",
            peer,
            state.seed,
            state.reported_version or "legacy",
        )

        while True:
            packet_size = PLAY_SERVER_PACKET_SIZE if state.authenticated else LOGIN_PACKET_SIZE
            try:
                plain, use_crypt = await read_login_packet(reader, state.seed, packet_size)
            except asyncio.IncompleteReadError:
                LOG.info("%s disconnected before sending %d-byte packet", peer, packet_size)
                break

            if not state.authenticated:
                state.use_login_crypt = use_crypt
                LOG.info("%s login transport: %s", peer, "encrypted" if use_crypt else "plaintext")

            if plain[0] == 0x80:
                parsed = parse_servers_req(plain)
                if parsed is None:
                    await send_login_error(
                        writer, state.seed, LOGIN_ERROR_OTHER, "malformed 0x80", peer, state.use_login_crypt
                    )
                    break

                account, password = parsed
                LOG.info("%s login attempt account=%r", peer, account)
                if not cfg.account_store.authenticate(account, password, cfg.md5_passwords):
                    await send_login_error(
                        writer, state.seed, LOGIN_ERROR_BAD_PASS, "bad password", peer, state.use_login_crypt
                    )
                    break

                state.account = account
                state.password = password
                state.authenticated = True

                shards = cfg.shard_registry.list_shards()
                if not shards:
                    await send_login_error(
                        writer, state.seed, LOGIN_ERROR_OTHER, "no shards configured", peer, state.use_login_crypt
                    )
                    break

                payload = build_server_list_packet(shards, cfg.shard_registry.host_to_ip_be)
                LOG.info(
                    "%s auth ok, sending server list (%d shard(s), %d bytes)",
                    peer,
                    len(shards),
                    len(payload),
                )
                await send_login_response(writer, state.seed, payload, state.use_login_crypt)
                continue

            if plain[0] == 0xA0 and len(plain) >= 3:
                if not state.authenticated:
                    await send_login_error(
                        writer, state.seed, LOGIN_ERROR_OTHER, "0xA0 before auth", peer, state.use_login_crypt
                    )
                    break

                server_index = parse_play_server(plain)
                if server_index is None:
                    await send_login_error(
                        writer, state.seed, LOGIN_ERROR_OTHER, "malformed 0xA0", peer, state.use_login_crypt
                    )
                    break

                shard = cfg.shard_registry.get_shard(server_index)
                if shard is None:
                    await send_login_error(
                        writer,
                        state.seed,
                        LOGIN_ERROR_OTHER,
                        f"invalid shard index {server_index}",
                        peer,
                        state.use_login_crypt,
                    )
                    break

                auth_id = make_auth_id(state.account, shard.name)
                LOG.info(
                    "%s selected shard %r (index=%d), registering session auth_id=0x%08X at %s",
                    peer,
                    shard.name,
                    server_index,
                    auth_id,
                    shard.internal_url,
                )
                ok = await asyncio.to_thread(
                    register_session,
                    shard.internal_url,
                    cfg.shared_secret,
                    state.account,
                    auth_id,
                    state.reported_version,
                    state.reported_version,
                )
                if not ok:
                    await send_login_error(
                        writer,
                        state.seed,
                        LOGIN_ERROR_OTHER,
                        f"session registration failed ({shard.internal_url})",
                        peer,
                        state.use_login_crypt,
                    )
                    break

                ip_le = cfg.shard_registry.host_to_ip_le(shard.host)
                relay = crypto.build_relay(ip_le, shard.port, auth_id)
                LOG.info(
                    "%s relay -> %s:%d auth_id=0x%08X packet=%s",
                    peer,
                    shard.host,
                    shard.port,
                    auth_id,
                    relay.hex(),
                )
                await send_login_response(writer, state.seed, relay, state.use_login_crypt)
                break

            await send_login_error(
                writer,
                state.seed,
                LOGIN_ERROR_OTHER,
                f"unexpected opcode 0x{plain[0]:02X}",
                peer,
                state.use_login_crypt,
            )
            break
    except asyncio.IncompleteReadError:
        LOG.info("%s disconnected during handshake", peer)
    except Exception as exc:
        LOG.exception("login client error from %s: %s", peer, exc)
    finally:
        writer.close()
        await writer.wait_closed()


async def run_server(cfg: LoginConfig) -> None:
    server = await asyncio.start_server(
        lambda r, w: handle_client(r, w, cfg),
        host=cfg.listen_host,
        port=cfg.listen_port,
    )
    poll_task = asyncio.create_task(cfg.shard_registry.poll_loop())
    addrs = ", ".join(str(sock.getsockname()) for sock in server.sockets or [])
    LOG.info("login server listening on %s", addrs)
    async with server:
        await server.serve_forever()
    poll_task.cancel()


def _resolve_path(path_str: str, config_path: Path) -> Path:
    path = Path(path_str)
    if not path.is_absolute():
        path = (config_path.parent / path).resolve()
    return path


def load_config(path: Path) -> LoginConfig:
    raw = json.loads(path.read_text(encoding="utf-8"))
    accounts_cfg = raw.get("accounts", {})
    scp_path = accounts_cfg.get("scp_path")
    store = AccountStore(
        scp_path=str(_resolve_path(scp_path, path)) if scp_path else None,
        sqlite_path=accounts_cfg.get("sqlite"),
        mysql_dsn=accounts_cfg.get("mysql"),
    )
    registry = ShardRegistry(
        config_path=path,
        shared_secret=raw["shared_secret"],
        poll_seconds=int(raw.get("status_poll_seconds", 30)),
    )
    return LoginConfig(
        listen_host=raw.get("listen_host", "0.0.0.0"),
        listen_port=int(raw.get("listen_port", 2592)),
        shared_secret=raw["shared_secret"],
        md5_passwords=bool(raw.get("md5_passwords", False)),
        account_store=store,
        shard_registry=registry,
    )


def main() -> None:
    parser = argparse.ArgumentParser(description="Source-X external login server")
    parser.add_argument(
        "-c",
        "--config",
        type=Path,
        default=Path(__file__).with_name("config.json"),
        help="Path to login server config JSON",
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Enable debug logging",
    )
    args = parser.parse_args()
    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(asctime)s %(levelname)s %(message)s",
        datefmt="%H:%M:%S",
    )
    cfg = load_config(args.config)
    LOG.info(
        "loaded %d accounts, %d shards (md5_passwords=%s)",
        cfg.account_store.count(),
        len(cfg.shard_registry.list_shards()),
        cfg.md5_passwords,
    )
    asyncio.run(run_server(cfg))


if __name__ == "__main__":
    main()
