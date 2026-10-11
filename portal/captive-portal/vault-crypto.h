// vault-crypto.h — the two crypto routines the charm needs. plain mbedtls, nothing clever.
// lives next to captive-portal.ino. the same file is compiled on a laptop to test it
// against the python script, which is why it has no arduino-specific code in it.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include "mbedtls/md.h"
#include "mbedtls/gcm.h"

// PBKDF2-HMAC-SHA256 for a single 32-byte output block (exactly one AES-256 key).
// written on top of mbedtls_md_hmac so it doesn't depend on mbedtls being built
// with its optional pkcs5 module. hmac_reset keeps the keyed state, so each round
// is one HMAC over the previous block.
static bool vault_pbkdf2(const uint8_t *pw, size_t pwLen,
                         const uint8_t *salt, size_t saltLen,
                         uint32_t iters, uint8_t out[32])
{
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  const mbedtls_md_info_t *info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
  if (!info || mbedtls_md_setup(&ctx, info, 1) != 0 || mbedtls_md_hmac_starts(&ctx, pw, pwLen) != 0) {
    mbedtls_md_free(&ctx);
    return false;
  }
  uint8_t u[32], t[32];
  const uint8_t block1[4] = {0, 0, 0, 1};      // big-endian block index, we only need block 1
  mbedtls_md_hmac_update(&ctx, salt, saltLen);
  mbedtls_md_hmac_update(&ctx, block1, 4);
  mbedtls_md_hmac_finish(&ctx, u);
  memcpy(t, u, 32);
  for (uint32_t i = 1; i < iters; i++) {
    mbedtls_md_hmac_reset(&ctx);
    mbedtls_md_hmac_update(&ctx, u, 32);
    mbedtls_md_hmac_finish(&ctx, u);
    for (int j = 0; j < 32; j++) t[j] ^= u[j];
  }
  memcpy(out, t, 32);
  mbedtls_md_free(&ctx);
  memset(u, 0, sizeof u);
  memset(t, 0, sizeof t);
  return true;
}

// AES-256-GCM decrypt + tag check. returns false (and leaves `out` as garbage) on a wrong key.
// `ct` must be in ram, not flash: the esp32's aes hardware can't dma out of the flash cache.
static bool vault_decrypt(const uint8_t key[32],
                          const uint8_t nonce[12], const uint8_t tag[16],
                          const uint8_t *ct, size_t ctLen, uint8_t *out)
{
  mbedtls_gcm_context g;
  mbedtls_gcm_init(&g);
  int rc = mbedtls_gcm_setkey(&g, MBEDTLS_CIPHER_ID_AES, key, 256);
  if (rc == 0) {
    rc = mbedtls_gcm_auth_decrypt(&g, ctLen, nonce, 12, NULL, 0, tag, 16, ct, out);
  }
  mbedtls_gcm_free(&g);
  return rc == 0;
}
