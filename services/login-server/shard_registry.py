"""Shard registry with optional heartbeat polling."""

from __future__ import annotations

import asyncio
import json
import time
import urllib.error
import urllib.request
from dataclasses import dataclass, field
from pathlib import Path
from typing import List, Optional


@dataclass
class ShardEntry:
    name: str
    host: str
    port: int
    internal_url: str
    timezone: int = 0
    percent_full: int = 0
    last_status_ms: float = field(default_factory=time.time)


class ShardRegistry:
    def __init__(self, config_path: Path, shared_secret: str, poll_seconds: int = 30):
        self._shared_secret = shared_secret
        self._poll_seconds = poll_seconds
        self._shards: List[ShardEntry] = []
        self._load_config(config_path)

    def _load_config(self, path: Path) -> None:
        raw = json.loads(path.read_text(encoding="utf-8"))
        for item in raw.get("shards", []):
            self._shards.append(
                ShardEntry(
                    name=item["name"],
                    host=item["host"],
                    port=int(item["port"]),
                    internal_url=item["internal_url"],
                    timezone=int(item.get("timezone", 0)),
                )
            )

    def list_shards(self) -> List[ShardEntry]:
        return list(self._shards)

    def get_shard(self, index: int) -> Optional[ShardEntry]:
        if index < 1 or index > len(self._shards):
            return None
        return self._shards[index - 1]

    def _fetch_status(self, shard: ShardEntry) -> None:
        url = shard.internal_url.rstrip("/") + "/internal/v1/status"
        req = urllib.request.Request(url, method="GET")
        req.add_header("Authorization", f"Bearer {self._shared_secret}")
        try:
            with urllib.request.urlopen(req, timeout=3) as resp:
                data = json.loads(resp.read().decode("utf-8"))
            if data.get("ok"):
                shard.percent_full = int(data.get("percent_full", 0))
                shard.last_status_ms = time.time()
        except (urllib.error.URLError, TimeoutError, json.JSONDecodeError):
            pass

    async def poll_loop(self) -> None:
        while True:
            for shard in self._shards:
                await asyncio.to_thread(self._fetch_status, shard)
            await asyncio.sleep(self._poll_seconds)

    @staticmethod
    def host_to_ip_be(host: str) -> int:
        """IPv4 for 0xA8 server-list entries (big-endian on the wire)."""
        parts = host.split(".")
        if len(parts) != 4:
            return 0x7F000001
        a, b, c, d = (int(x) for x in parts)
        return (a << 24) | (b << 16) | (c << 8) | d

    @staticmethod
    def host_to_ip_le(host: str) -> int:
        """IPv4 dword for 0x8C relay (Windows sockaddr / ClassicUO IPAddress layout).

        Sphere PacketServerRelay writes the low byte of this dword first on the wire.
        ClassicUO reads it as UInt32LE and passes it to ``new IPAddress(ip)``.
        """
        parts = host.split(".")
        if len(parts) != 4:
            return 0x0100007F  # inet_addr("127.0.0.1") / SOCKET_LOCAL_ADDRESS
        a, b, c, d = (int(x) for x in parts)
        return (d << 24) | (c << 16) | (b << 8) | a
