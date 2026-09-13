"""ctypes bindings for sphere_login_crypto shared library."""

from __future__ import annotations

import ctypes
import os
import platform
import struct
from pathlib import Path
from typing import Optional

_LIB: Optional[ctypes.CDLL] = None


def _library_name() -> str:
    if platform.system() == "Windows":
        return "sphere_login_crypto.dll"
    if platform.system() == "Darwin":
        return "libsphere_login_crypto.dylib"
    return "libsphere_login_crypto.so"


def _search_paths() -> list[Path]:
    here = Path(__file__).resolve().parent
    root = here.parent.parent
    candidates = [
        here,
        here.parent / "login-crypto" / "build",
        root / "build" / "services" / "login-crypto",
        root / "build" / "Release",
        root / "build" / "Debug",
    ]
    env = os.environ.get("SPHERE_LOGIN_CRYPTO_PATH")
    if env:
        candidates.insert(0, Path(env))
    return candidates


def load_library(force: bool = False) -> Optional[ctypes.CDLL]:
    global _LIB
    if _LIB is not None and not force:
        return _LIB

    name = _library_name()
    for directory in _search_paths():
        path = directory / name
        if path.is_file():
            _LIB = ctypes.CDLL(str(path))
            _bind(_LIB)
            return _LIB
    return None


def _bind(lib: ctypes.CDLL) -> None:
    lib.sphere_build_server_list.argtypes = [
        ctypes.c_uint16,
        ctypes.c_char_p,
        ctypes.c_uint8,
        ctypes.c_int8,
        ctypes.c_uint32,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    ]
    lib.sphere_build_server_list.restype = ctypes.c_int

    lib.sphere_build_relay.argtypes = [
        ctypes.c_uint32,
        ctypes.c_uint16,
        ctypes.c_uint32,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    ]
    lib.sphere_build_relay.restype = ctypes.c_int

    lib.sphere_build_login_error.argtypes = [
        ctypes.c_uint8,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_size_t),
    ]
    lib.sphere_build_login_error.restype = ctypes.c_int

    lib.sphere_login_decrypt_legacy.argtypes = [
        ctypes.c_uint32,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
    ]
    lib.sphere_login_decrypt_legacy.restype = ctypes.c_int

    lib.sphere_login_encrypt_legacy.argtypes = [
        ctypes.c_uint32,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
        ctypes.POINTER(ctypes.c_uint8),
        ctypes.c_size_t,
    ]
    lib.sphere_login_encrypt_legacy.restype = ctypes.c_int


def build_server_entry(index: int, name: str, percent: int, timezone: int, ip_be: int) -> bytes:
    """Per-shard body inside a 0xA8 server-list packet (index, name, load, tz, ip)."""
    name_bytes = name.encode("ascii", errors="ignore")[:31]
    name_field = name_bytes.ljust(32, b"\x00")
    payload = bytearray()
    payload.extend(struct.pack(">H", index))
    payload.extend(name_field)
    payload.append(percent & 0xFF)
    payload.append(timezone & 0xFF)
    payload.extend(struct.pack(">I", ip_be))
    return bytes(payload)


def build_server_list_packet(entries: list[bytes]) -> bytes:
    """Build a complete variable-length 0xA8 server-list packet."""
    body = bytearray([0xFF])
    body.extend(struct.pack(">H", len(entries)))
    for entry in entries:
        body.extend(entry)
    packet = bytearray([0xA8])
    packet.extend(struct.pack(">H", 3 + len(body)))
    packet.extend(body)
    return bytes(packet)


def _fallback_build_server_list(index: int, name: str, percent: int, timezone: int, ip_be: int) -> bytes:
    return build_server_list_packet([build_server_entry(index, name, percent, timezone, ip_be)])


def _fallback_build_relay(ip_le: int, port: int, auth_id: int) -> bytes:
    # IP as LE dword bytes; port and auth id big-endian (Sphere PacketServerRelay / ClassicUO).
    body = struct.pack("<I", ip_le)
    body += struct.pack(">H", port)
    body += struct.pack(">I", auth_id)
    return bytes([0x8C]) + body


def _relay_matches_classicuo(packet: bytes, port: int, auth_id: int) -> bool:
    """ClassicUO reads relay port/auth as big-endian (see LoginScene.HandleRelayServerPacket)."""
    if len(packet) != 11 or packet[0] != 0x8C:
        return False
    port_be = struct.unpack(">H", packet[5:7])[0]
    auth_be = struct.unpack(">I", packet[7:11])[0]
    return port_be == port and auth_be == auth_id


def build_server_list(index: int, name: str, percent: int, timezone: int, ip_be: int) -> bytes:
    lib = load_library()
    if lib is None:
        return _fallback_build_server_list(index, name, percent, timezone, ip_be)

    out = (ctypes.c_uint8 * 512)()
    out_len = ctypes.c_size_t(0)
    rc = lib.sphere_build_server_list(
        ctypes.c_uint16(index),
        name.encode("ascii"),
        ctypes.c_uint8(percent),
        ctypes.c_int8(timezone),
        ctypes.c_uint32(ip_be),
        out,
        ctypes.c_size_t(len(out)),
        ctypes.byref(out_len),
    )
    if rc != 0:
        return _fallback_build_server_list(index, name, percent, timezone, ip_be)
    return bytes(out[: out_len.value])


def build_relay(ip_le: int, port: int, auth_id: int) -> bytes:
    lib = load_library()
    if lib is not None:
        out = (ctypes.c_uint8 * 64)()
        out_len = ctypes.c_size_t(0)
        rc = lib.sphere_build_relay(
            ctypes.c_uint32(ip_le),
            ctypes.c_uint16(port),
            ctypes.c_uint32(auth_id),
            out,
            ctypes.c_size_t(len(out)),
            ctypes.byref(out_len),
        )
        if rc == 0:
            packet = bytes(out[: out_len.value])
            if _relay_matches_classicuo(packet, port, auth_id):
                return packet

    return _fallback_build_relay(ip_le, port, auth_id)


def build_login_error(code: int) -> bytes:
    lib = load_library()
    if lib is None:
        return bytes([0x82, code & 0xFF])

    out = (ctypes.c_uint8 * 8)()
    out_len = ctypes.c_size_t(0)
    rc = lib.sphere_build_login_error(
        ctypes.c_uint8(code),
        out,
        ctypes.c_size_t(len(out)),
        ctypes.byref(out_len),
    )
    if rc != 0:
        return bytes([0x82, code & 0xFF])
    return bytes(out[: out_len.value])


LOGIN_PACKET_OPCODES = frozenset({0x80, 0xA0, 0xA4, 0x82, 0xA8, 0x8C})


def looks_like_login_plaintext(data: bytes) -> bool:
    return bool(data) and data[0] in LOGIN_PACKET_OPCODES


def decode_login_packet(seed: int, data: bytes) -> tuple[bytes, bool]:
    """Return (plaintext, uses_login_crypt). ClassicUO with encryption=0 sends plaintext."""
    if looks_like_login_plaintext(data):
        return data, False
    plain = login_decrypt_legacy(seed, data)
    if looks_like_login_plaintext(plain):
        return plain, True
    return plain, True


def encode_login_packet(seed: int, data: bytes, uses_login_crypt: bool) -> bytes:
    if uses_login_crypt:
        return login_encrypt_legacy(seed, data)
    return data


def login_decrypt_legacy(seed: int, data: bytes) -> bytes:
    return _python_login_crypt(seed, data, encrypt=False)


def login_encrypt_legacy(seed: int, data: bytes) -> bytes:
    return _python_login_crypt(seed, data, encrypt=True)


def _python_login_crypt(seed: int, data: bytes, encrypt: bool) -> bytes:
    key_lo = (((~seed) & 0xFFFFFFFF) ^ 0x00001357) << 16
    key_lo = (key_lo | ((seed ^ 0xFFFFAAAA) & 0x0000FFFF)) & 0xFFFFFFFF
    key_hi = (((seed ^ 0x43210000) & 0xFFFFFFFF) >> 16) | (((~seed) & 0xFFFFFFFF) ^ 0xABCFFFFF) & 0xFFFF0000
    key_hi &= 0xFFFFFFFF

    out = bytearray()
    for byte in data:
        if encrypt:
            mixed = (key_lo ^ byte) & 0xFF
            out.append(mixed)
        else:
            out.append((key_lo ^ byte) & 0xFF)
        old_lo, old_hi = key_lo, key_hi
        key_lo = (((old_lo >> 1) | ((old_hi & 1) << 31)) ^ 0x3A1FD527) & 0xFFFFFFFF
        key_hi = (((old_hi >> 1) | ((old_lo & 1) << 31)) ^ 0xF1A372D5) & 0xFFFFFFFF
    return bytes(out)
