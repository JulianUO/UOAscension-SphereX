#!/usr/bin/env python3
"""Import accounts from sphereacct.scp into SQLite or MariaDB."""

from __future__ import annotations

import argparse
import sqlite3
import sys
from pathlib import Path

try:
    import pymysql
except ImportError:
    pymysql = None

from accounts import AccountStore

SQLITE_SCHEMA = Path(__file__).with_name("sql") / "accounts_sqlite.sql"


def import_sqlite(store: AccountStore, db_path: Path) -> None:
    db_path.parent.mkdir(parents=True, exist_ok=True)
    conn = sqlite3.connect(db_path)
    try:
        conn.executescript(SQLITE_SCHEMA.read_text(encoding="utf-8"))
        for rec in store._accounts.values():
            conn.execute(
                """
                INSERT INTO accounts (name, password, blocked)
                VALUES (?, ?, ?)
                ON CONFLICT(name) DO UPDATE SET
                    password=excluded.password,
                    blocked=excluded.blocked,
                    updated_at=CURRENT_TIMESTAMP
                """,
                (rec.name, rec.password, int(rec.blocked)),
            )
        conn.commit()
    finally:
        conn.close()


def import_mysql(store: AccountStore, args: argparse.Namespace) -> None:
    if pymysql is None:
        print("pymysql required: pip install pymysql", file=sys.stderr)
        sys.exit(1)
    conn = pymysql.connect(
        host=args.host,
        user=args.user,
        password=args.password,
        database=args.database,
        port=args.port,
        charset="utf8mb4",
        autocommit=False,
    )
    try:
        with conn.cursor() as cur:
            for rec in store._accounts.values():
                cur.execute(
                    """
                    INSERT INTO accounts (name, password, blocked)
                    VALUES (%s, %s, %s)
                    ON DUPLICATE KEY UPDATE password=VALUES(password), blocked=VALUES(blocked)
                    """,
                    (rec.name, rec.password, int(rec.blocked)),
                )
        conn.commit()
    finally:
        conn.close()


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("scp_path", type=Path)
    parser.add_argument("--sqlite", type=Path, help="Write accounts to a SQLite database file")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=3306)
    parser.add_argument("--user")
    parser.add_argument("--password", default="")
    parser.add_argument("--database")
    args = parser.parse_args()

    if args.sqlite:
        target = "sqlite"
    elif args.user and args.database:
        target = "mysql"
    else:
        parser.error("pass --sqlite PATH or both --user and --database for MariaDB")

    store = AccountStore(scp_path=str(args.scp_path))
    if target == "sqlite":
        import_sqlite(store, args.sqlite)
    else:
        import_mysql(store, args)

    print(f"imported {store.count()} accounts")


if __name__ == "__main__":
    main()
