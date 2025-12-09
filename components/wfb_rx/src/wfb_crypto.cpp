/**
 * @file wfb_crypto.cpp
 * @brief ChaCha20-Poly1305 AEAD Decryption Implementation
 *
 * Uses mbedtls (available in ESP-IDF) for cryptographic operations.
 */

#include "wfb_crypto.h"
#include <cstring>
#include <cstdio>

#ifdef ESP_PLATFORM
#include "mbedtls/chachapoly.h"
#include "mbedtls/chacha20.h"
#include "mbedtls/poly1305.h"
#include "mbedtls/curve25519.h"
#include "mbedtls/hkdf.h"
#include "mbedtls/sha512.h"
#include "esp_log.h"
static const char* TAG = "wfb_crypto";
#define LOG_E(fmt, ...) ESP_LOGE(TAG, fmt, ##__VA_ARGS__)
#define LOG_W(fmt, ...) ESP_LOGW(TAG, fmt, ##__VA_ARGS__)
#define LOG_I(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
#define LOG_D(fmt, ...) ESP_LOGD(TAG, fmt, ##__VA_ARGS__)
#else
#include <openssl/evp.h>
#define LOG_E(fmt, ...) fprintf(stderr, "[E] " fmt "\n", ##__VA_ARGS__)
#define LOG_W(fmt, ...) fprintf(stderr, "[W] " fmt "\n", ##__VA_ARGS__)
#define LOG_I(fmt, ...) printf("[I] " fmt "\n", ##__VA_ARGS__)
#define LOG_D(fmt, ...) // printf("[D] " fmt "\n", ##__VA_ARGS__)
#endif

// ============================================================================
// Initialization
// ============================================================================

void wfb_crypto_init(wfb_crypto_ctx_t* ctx) {
    if (!ctx) return;
    memset(ctx, 0, sizeof(wfb_crypto_ctx_t));
    ctx->session_valid = false;
    ctx->gs_keys_loaded = false;
    ctx->fec_k = WFB_DEFAULT_FEC_K;
    ctx->fec_n = WFB_DEFAULT_FEC_N;
}

bool wfb_crypto_load_keypair(wfb_crypto_ctx_t* ctx,
                              const uint8_t* public_key,
                              const uint8_t* secret_key) {
    if (!ctx || !public_key || !secret_key) return false;

    memcpy(ctx->gs_public_key, public_key, WFB_CRYPTO_BOX_PUBLICKEYBYTES);
    memcpy(ctx->gs_secret_key, secret_key, WFB_CRYPTO_BOX_SECRETKEYBYTES);
    ctx->gs_keys_loaded = true;

    LOG_I("Ground station keypair loaded");
    return true;
}

bool wfb_crypto_load_gs_key(wfb_crypto_ctx_t* ctx, const uint8_t* key_data) {
    if (!ctx || !key_data) return false;

    // gs.key format: [secret_key(32)][public_key(32)]
    const uint8_t* secret_key = key_data;
    const uint8_t* public_key = key_data + WFB_CRYPTO_BOX_SECRETKEYBYTES;

    return wfb_crypto_load_keypair(ctx, public_key, secret_key);
}

// ============================================================================
// ChaCha20-Poly1305 Decryption
// ============================================================================

#ifdef ESP_PLATFORM

int wfb_chacha20poly1305_decrypt(uint8_t* plaintext,
                                  size_t* plaintext_len,
                                  const uint8_t* ciphertext,
                                  size_t ciphertext_len,
                                  const uint8_t* aad,
                                  size_t aad_len,
                                  const uint8_t* nonce,
                                  const uint8_t* key) {
    if (ciphertext_len < WFB_POLY1305_TAG_SIZE) {
        return -1;
    }

    size_t actual_ciphertext_len = ciphertext_len - WFB_POLY1305_TAG_SIZE;
    const uint8_t* tag = ciphertext + actual_ciphertext_len;

    mbedtls_chachapoly_context ctx;
    mbedtls_chachapoly_init(&ctx);

    int ret = mbedtls_chachapoly_setkey(&ctx, key);
    if (ret != 0) {
        LOG_E("ChaCha20-Poly1305 setkey failed: %d", ret);
        mbedtls_chachapoly_free(&ctx);
        return -1;
    }

    ret = mbedtls_chachapoly_auth_decrypt(&ctx,
                                           actual_ciphertext_len,
                                           nonce,
                                           aad, aad_len,
                                           tag,
                                           ciphertext,
                                           plaintext);

    mbedtls_chachapoly_free(&ctx);

    if (ret != 0) {
        LOG_D("ChaCha20-Poly1305 auth failed: %d", ret);
        return -1;
    }

    *plaintext_len = actual_ciphertext_len;
    return 0;
}

#else
// Desktop/Linux implementation using OpenSSL

int wfb_chacha20poly1305_decrypt(uint8_t* plaintext,
                                  size_t* plaintext_len,
                                  const uint8_t* ciphertext,
                                  size_t ciphertext_len,
                                  const uint8_t* aad,
                                  size_t aad_len,
                                  const uint8_t* nonce,
                                  const uint8_t* key) {
    if (ciphertext_len < WFB_POLY1305_TAG_SIZE) {
        return -1;
    }

    size_t actual_ciphertext_len = ciphertext_len - WFB_POLY1305_TAG_SIZE;
    const uint8_t* tag = ciphertext + actual_ciphertext_len;

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return -1;

    int ret = -1;
    int len;

    if (EVP_DecryptInit_ex(ctx, EVP_chacha20_poly1305(), NULL, NULL, NULL) != 1) goto cleanup;
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_IVLEN, 12, NULL) != 1) goto cleanup;
    if (EVP_DecryptInit_ex(ctx, NULL, NULL, key, nonce) != 1) goto cleanup;

    if (aad && aad_len > 0) {
        if (EVP_DecryptUpdate(ctx, NULL, &len, aad, aad_len) != 1) goto cleanup;
    }

    if (EVP_DecryptUpdate(ctx, plaintext, &len, ciphertext, actual_ciphertext_len) != 1) goto cleanup;
    *plaintext_len = len;

    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_AEAD_SET_TAG, WFB_POLY1305_TAG_SIZE, (void*)tag) != 1) goto cleanup;

    if (EVP_DecryptFinal_ex(ctx, plaintext + len, &len) != 1) goto cleanup;
    *plaintext_len += len;

    ret = 0;

cleanup:
    EVP_CIPHER_CTX_free(ctx);
    return ret;
}

#endif

// ============================================================================
// Data Packet Decryption
// ============================================================================

bool wfb_crypto_decrypt(wfb_crypto_ctx_t* ctx,
                         const uint8_t* ciphertext,
                         size_t ciphertext_len,
                         uint64_t nonce,
                         uint8_t* plaintext,
                         size_t* plaintext_len) {
    if (!ctx || !ctx->session_valid) {
        LOG_D("Decrypt called without valid session");
        return false;
    }

    if (ciphertext_len < WFB_POLY1305_TAG_SIZE) {
        LOG_D("Ciphertext too short: %zu", ciphertext_len);
        return false;
    }

    // Build 12-byte IETF ChaCha20-Poly1305 nonce
    // WFB-NG uses 8-byte nonce, pad with zeros to 12 bytes
    uint8_t full_nonce[12] = {0};
    memcpy(full_nonce, &nonce, 8);

    int ret = wfb_chacha20poly1305_decrypt(
        plaintext,
        plaintext_len,
        ciphertext,
        ciphertext_len,
        NULL, 0,  // No AAD
        full_nonce,
        ctx->session_key
    );

    if (ret != 0) {
        LOG_D("Decryption failed for nonce %" PRIu64, nonce);
        return false;
    }

    return true;
}

// ============================================================================
// Session Key Processing
// ============================================================================

#ifdef ESP_PLATFORM

// Simplified X25519 + HSalsa20 for crypto_box_open
// Note: Full NaCl crypto_box uses X25519 for key exchange, then XSalsa20-Poly1305
// ESP32 mbedtls has X25519 support

static bool compute_shared_secret(uint8_t* shared_key,
                                   const uint8_t* sender_pk,
                                   const uint8_t* receiver_sk) {
    // X25519 key exchange
    mbedtls_ecdh_context ecdh;
    mbedtls_ecdh_init(&ecdh);

    // This is a simplified implementation - full NaCl uses HSalsa20 on the result
    // For now, we use the raw X25519 output (not fully compatible with libsodium)

    // Note: Proper implementation would require:
    // 1. X25519(receiver_sk, sender_pk) -> shared_point
    // 2. HSalsa20(shared_point, zero_nonce) -> shared_key

    // Placeholder - needs proper implementation with HSalsa20
    LOG_W("Session key decryption not fully implemented on ESP32");
    mbedtls_ecdh_free(&ecdh);
    return false;
}

#endif

bool wfb_crypto_process_session(wfb_crypto_ctx_t* ctx,
                                 const uint8_t* session_pkt,
                                 size_t pkt_len,
                                 const uint8_t* tx_public_key) {
    if (!ctx || !session_pkt || !tx_public_key) {
        return false;
    }

    if (!ctx->gs_keys_loaded) {
        LOG_W("Cannot process session: GS keys not loaded");
        return false;
    }

    // Session packet structure:
    // [packet_type(1)][session_nonce(24)][encrypted_session_data + tag]
    if (pkt_len < sizeof(wfb_session_hdr_t) + sizeof(wfb_session_data_t) + WFB_CRYPTO_BOX_MACBYTES) {
        LOG_W("Session packet too short: %zu", pkt_len);
        return false;
    }

    const wfb_session_hdr_t* hdr = (const wfb_session_hdr_t*)session_pkt;

    if (hdr->packet_type != WFB_PACKET_SESSION) {
        LOG_W("Not a session packet: type=%d", hdr->packet_type);
        return false;
    }

    // Decrypt session data using crypto_box_open
    const uint8_t* encrypted = session_pkt + sizeof(wfb_session_hdr_t);
    size_t encrypted_len = pkt_len - sizeof(wfb_session_hdr_t);

    wfb_session_data_t session_data;

    int ret = wfb_crypto_box_open(
        (uint8_t*)&session_data,
        encrypted,
        encrypted_len,
        hdr->session_nonce,
        tx_public_key,
        ctx->gs_secret_key
    );

    if (ret != 0) {
        LOG_W("Session key decryption failed");
        return false;
    }

    // Validate and store session data
    if (session_data.fec_type != WFB_FEC_VDM_RS) {
        LOG_W("Unknown FEC type: %d", session_data.fec_type);
        return false;
    }

    // Check if this is a newer session
    if (ctx->session_valid && session_data.epoch <= ctx->session_epoch) {
        LOG_D("Ignoring old session epoch %" PRIu64, session_data.epoch);
        return true;  // Not an error, just old data
    }

    // Update session
    memcpy(ctx->session_key, session_data.session_key, WFB_CHACHA20_KEY_SIZE);
    ctx->session_epoch = session_data.epoch;
    ctx->channel_id = session_data.channel_id;
    ctx->fec_k = session_data.fec_k;
    ctx->fec_n = session_data.fec_n;
    ctx->session_valid = true;

    LOG_I("New session: epoch=%" PRIu64 " ch=0x%08X k=%d n=%d",
          ctx->session_epoch, ctx->channel_id, ctx->fec_k, ctx->fec_n);

    return true;
}

// ============================================================================
// Crypto Box (Public Key Decryption)
// ============================================================================

int wfb_crypto_box_open(uint8_t* plaintext,
                         const uint8_t* ciphertext,
                         size_t ciphertext_len,
                         const uint8_t* nonce,
                         const uint8_t* sender_pk,
                         const uint8_t* receiver_sk) {
#ifdef ESP_PLATFORM
    // ESP32: Use mbedtls X25519 + XSalsa20-Poly1305
    // This requires implementing HSalsa20 for the key derivation

    // For now, return error - needs proper implementation
    // Full implementation would:
    // 1. X25519(receiver_sk, sender_pk) -> shared_point
    // 2. HSalsa20(shared_point, first_16_bytes_of_nonce) -> key
    // 3. XSalsa20-Poly1305-decrypt(ciphertext, remaining_nonce, key)

    LOG_W("crypto_box_open not yet implemented for ESP32");
    (void)plaintext;
    (void)ciphertext;
    (void)ciphertext_len;
    (void)nonce;
    (void)sender_pk;
    (void)receiver_sk;
    return -1;
#else
    // Desktop: Could use libsodium directly if available
    // For testing, placeholder implementation
    (void)plaintext;
    (void)ciphertext;
    (void)ciphertext_len;
    (void)nonce;
    (void)sender_pk;
    (void)receiver_sk;
    return -1;
#endif
}
