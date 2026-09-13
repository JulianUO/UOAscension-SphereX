"""Load accounts from sphereacct.scp, SQLite, or MariaDB."""

from __future__ import annotations

import hashlib
import sqlite3
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional, Union

try:
    import pymysql
except ImportError:  # pragma: no cover - optional dependency
    pymysql = None


@dataclass
class AccountRecord:
    name: str
    password: str
    blocked: bool = False


class AccountStore:
    def __init__(
        self,
        scp_path: Optional[str] = None,
        sqlite_path: Optional[Union[str, dict]] = None,
        mysql_dsn: Optional[dict] = None,
    ):
        self._accounts: Dict[str, AccountRecord] = {}
        if scp_path:
            self._load_scp(Path(scp_path))
        if sqlite_path:
            path = sqlite_path if isinstance(sqlite_path, str) else sqlite_path["path"]
            self._load_sqlite(Path(path))
        if mysql_dsn:
            self._load_mysql(mysql_dsn)

    def _load_scp(self, path: Path) -> None:
        if not path.is_file():
            raise FileNotFoundError(path)
        text = path.read_text(encoding="latin-1", errors="ignore")
        current: Optional[AccountRecord] = None
        for raw_line in text.splitlines():
            line = raw_line.strip()
            if not line or line.startswith("//"):
                continue
            if line.startswith("[") and line.endswith("]"):
                inner = line[1:-1].strip()
                if inner.upper().startswith("ACCOUNT "):
                    name = inner[8:].strip()
                else:
                    name = inner
                current = AccountRecord(name=name, password="")
                self._accounts[name.lower()] = current
                continue
            if current is None:
                continue
            key, _, value = line.partition("=")
            key = key.strip().upper()
            value = value.strip().strip('"')
            if key == "PASSWORD":
                current.password = value
            elif key == "BLOCK" and value not in ("0", ""):
                current.blocked = True
            elif key == "PRIV" and value.startswith("02000"):
                current.blocked = True

    def _load_sqlite(self, path: Path) -> None:
        if not path.is_file():
            raise FileNotFoundError(path)
        conn = sqlite3.connect(path)
        try:
            row = conn.execute(
                "SELECT name FROM sqlite_master WHERE type='table' AND name='accounts'"
            ).fetchone()
            if row is None:
                raise RuntimeError(f"SQLite database missing accounts table: {path}")
            for name, password, blocked in conn.execute(
                "SELECT name, password, blocked FROM accounts"
            ):
                self._accounts[str(name).lower()] = AccountRecord(
                    name=str(name),
                    password=str(password or ""),
                    blocked=bool(blocked),
                )
        finally:
            conn.close()

    def _load_mysql(self, dsn: dict) -> None:
        if pymysql is None:
            raise RuntimeError("pymysql is required for MariaDB account storage")
        conn = pymysql.connect(
            host=dsn.get("host", "127.0.0.1"),
            user=dsn["user"],
            password=dsn.get("password", ""),
            database=dsn["database"],
            port=int(dsn.get("port", 3306)),
            charset="utf8mb4",
            autocommit=True,
        )
        try:
            with conn.cursor() as cur:
                cur.execute("SELECT name, password, blocked FROM accounts")
                for name, password, blocked in cur.fetchall():
                    self._accounts[str(name).lower()] = AccountRecord(
                        name=str(name),
                        password=str(password or ""),
                        blocked=bool(blocked),
                    )
        finally:
            conn.close()

    def authenticate(self, account: str, password: str, md5_passwords: bool = False) -> bool:
        rec = self._accounts.get(account.lower())
        if rec is None or rec.blocked:
            return False
        if not md5_passwords:
            return rec.password == password
        digest = hashlib.md5(password.encode("utf-8")).hexdigest().upper()
        stored = rec.password.upper()
        return stored == digest or stored == password

    def count(self) -> int:
        return len(self._accounts)
