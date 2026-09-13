#define SPHERE_LOGIN_CRYPTO_EXPORTS
#include "sphere_login_crypto.h"

#include <cstring>

namespace
{
    int write_packet(uint8_t cmd, const uint8_t* payload, size_t payload_len, uint8_t* out, size_t out_cap, size_t* out_len)
    {
        const size_t total = 1 + payload_len;
        if (total > out_cap)
            return -1;
        out[0] = cmd;
        if (payload_len > 0)
            memcpy(out + 1, payload, payload_len);
        *out_len = total;
        return 0;
    }

    void login_crypt_transform(uint32_t seed, uint8_t* buffer, size_t len, bool encrypt)
    {
        uint32_t keyLo = (((~seed) ^ 0x00001357U) << 16) | ((seed ^ 0xffffaaaaU) & 0x0000ffffU);
        uint32_t keyHi = ((seed ^ 0x43210000U) >> 16) | (((~seed) ^ 0xabcdffffU) & 0xffff0000U);

    for (size_t i = 0; i < len; ++i)
    {
        const uint8_t byte = buffer[i];
        if (encrypt)
            buffer[i] = static_cast<uint8_t>(keyLo ^ byte);
        else
            buffer[i] = static_cast<uint8_t>(keyLo ^ byte);

        const uint32_t oldLo = keyLo;
        const uint32_t oldHi = keyHi;
        keyLo = ((oldLo >> 1) | (oldHi << 31)) ^ 0x3A1FD527U;
        keyHi = ((oldHi >> 1) | (oldLo << 31)) ^ 0xF1A372D5U;
    }
    }
}

extern "C" int sphere_build_server_list(
    uint16_t index,
    const char* name,
    uint8_t percent_full,
    int8_t timezone,
    uint32_t ip_be,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len)
{
    if (!name || !out || !out_len)
        return -1;

    uint8_t payload[256];
    size_t pos = 0;
    payload[pos++] = 0xFF;
    payload[pos++] = 0x01;
    payload[pos++] = 0x00;
    payload[pos++] = static_cast<uint8_t>(index & 0xFF);
    payload[pos++] = static_cast<uint8_t>((index >> 8) & 0xFF);

    char nameField[32];
    memset(nameField, 0, sizeof(nameField));
    strncpy(nameField, name, sizeof(nameField) - 1);
    memcpy(payload + pos, nameField, 32);
    pos += 32;

    payload[pos++] = percent_full;
    payload[pos++] = static_cast<uint8_t>(timezone);

    payload[pos++] = static_cast<uint8_t>((ip_be >> 24) & 0xFF);
    payload[pos++] = static_cast<uint8_t>((ip_be >> 16) & 0xFF);
    payload[pos++] = static_cast<uint8_t>((ip_be >> 8) & 0xFF);
    payload[pos++] = static_cast<uint8_t>(ip_be & 0xFF);

    return write_packet(0xA8, payload, pos, out, out_cap, out_len);
}

extern "C" int sphere_build_relay(
    uint32_t ip_le,
    uint16_t port,
    uint32_t auth_id,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len)
{
    uint8_t payload[10];
    payload[0] = static_cast<uint8_t>(ip_le & 0xFF);
    payload[1] = static_cast<uint8_t>((ip_le >> 8) & 0xFF);
    payload[2] = static_cast<uint8_t>((ip_le >> 16) & 0xFF);
    payload[3] = static_cast<uint8_t>((ip_le >> 24) & 0xFF);
    // Match PacketServerRelay / ClassicUO: port and auth id are big-endian on the wire.
    payload[4] = static_cast<uint8_t>((port >> 8) & 0xFF);
    payload[5] = static_cast<uint8_t>(port & 0xFF);
    payload[6] = static_cast<uint8_t>((auth_id >> 24) & 0xFF);
    payload[7] = static_cast<uint8_t>((auth_id >> 16) & 0xFF);
    payload[8] = static_cast<uint8_t>((auth_id >> 8) & 0xFF);
    payload[9] = static_cast<uint8_t>(auth_id & 0xFF);
    return write_packet(0x8C, payload, sizeof(payload), out, out_cap, out_len);
}

extern "C" int sphere_build_login_error(uint8_t code, uint8_t* out, size_t out_cap, size_t* out_len)
{
    return write_packet(0x82, &code, 1, out, out_cap, out_len);
}

extern "C" int sphere_login_decrypt_legacy(uint32_t seed, const uint8_t* in, size_t in_len, uint8_t* out, size_t out_cap)
{
    if (!in || !out || in_len == 0 || in_len > out_cap)
        return -1;
    memcpy(out, in, in_len);
    login_crypt_transform(seed, out, in_len, false);
    return static_cast<int>(in_len);
}

extern "C" int sphere_login_encrypt_legacy(uint32_t seed, const uint8_t* in, size_t in_len, uint8_t* out, size_t out_cap)
{
    if (!in || !out || in_len == 0 || in_len > out_cap)
        return -1;
    memcpy(out, in, in_len);
    login_crypt_transform(seed, out, in_len, true);
    return static_cast<int>(in_len);
}
