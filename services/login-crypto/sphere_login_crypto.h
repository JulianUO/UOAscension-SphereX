#ifndef SPHERE_LOGIN_CRYPTO_H
#define SPHERE_LOGIN_CRYPTO_H

#include <stddef.h>
#include <stdint.h>

#if defined(_WIN32)
#  if defined(SPHERE_LOGIN_CRYPTO_EXPORTS)
#    define SPHERE_LOGIN_CRYPTO_API __declspec(dllexport)
#  else
#    define SPHERE_LOGIN_CRYPTO_API __declspec(dllimport)
#  endif
#else
#  define SPHERE_LOGIN_CRYPTO_API __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* Build 0xA8 server list (single shard entry). reverse_ip: ClassicUO pre-4.0.0 clients. */
SPHERE_LOGIN_CRYPTO_API int sphere_build_server_list(
    uint16_t index,
    const char* name,
    uint8_t percent_full,
    int8_t timezone,
    uint32_t ip_be,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len);

/* Build 0x8C relay packet. ip bytes are little-endian wire order (same as Sphere PacketServerRelay). */
SPHERE_LOGIN_CRYPTO_API int sphere_build_relay(
    uint32_t ip_le,
    uint16_t port,
    uint32_t auth_id,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len);

/* Build 0x82 login error (1 byte code). */
SPHERE_LOGIN_CRYPTO_API int sphere_build_login_error(
    uint8_t code,
    uint8_t* out,
    size_t out_cap,
    size_t* out_len);

/* Legacy login-stream XOR decrypt (pre-Blowfish handshake). Returns decrypted length or negative error. */
SPHERE_LOGIN_CRYPTO_API int sphere_login_decrypt_legacy(
    uint32_t seed,
    const uint8_t* in,
    size_t in_len,
    uint8_t* out,
    size_t out_cap);

/* Legacy login-stream XOR encrypt for server->client login packets. */
SPHERE_LOGIN_CRYPTO_API int sphere_login_encrypt_legacy(
    uint32_t seed,
    const uint8_t* in,
    size_t in_len,
    uint8_t* out,
    size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif
