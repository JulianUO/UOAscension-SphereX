"""Register login sessions on game shards."""

from __future__ import annotations

import json
import logging
import urllib.error
import urllib.request

LOG = logging.getLogger("login_server")


def register_session(
    internal_url: str,
    shared_secret: str,
    account: str,
    auth_id: int,
    client_version: int = 0,
    reported_version: int = 0,
    ttl_seconds: int = 30,
) -> bool:
    body = json.dumps(
        {
            "account": account,
            "auth_id": auth_id,
            "client_version": client_version,
            "reported_version": reported_version,
            "ttl_seconds": ttl_seconds,
        }
    ).encode("utf-8")
    url = internal_url.rstrip("/") + "/internal/v1/sessions"
    req = urllib.request.Request(url, data=body, method="POST")
    req.add_header("Content-Type", "application/json")
    req.add_header("Authorization", f"Bearer {shared_secret}")
    try:
        with urllib.request.urlopen(req, timeout=3) as resp:
            data = json.loads(resp.read().decode("utf-8"))
            ok = bool(data.get("ok"))
            if not ok:
                LOG.warning("session registration rejected by %s: %s", url, data)
            return ok
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        LOG.warning("session registration HTTP %s from %s: %s", exc.code, url, body)
        return False
    except (urllib.error.URLError, TimeoutError, json.JSONDecodeError) as exc:
        LOG.warning("session registration failed for %s: %s", url, exc)
        return False
