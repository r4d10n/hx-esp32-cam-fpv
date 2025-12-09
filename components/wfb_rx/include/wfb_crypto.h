/**
 * @file wfb_crypto.h
 * @brief ChaCha20-Poly1305 AEAD Decryption for WFB-NG
 *
 * Implements ChaCha20-Poly1305 authenticated encryption/decryption
 * compatible with libsodium's crypto_aead_chacha20poly1305_ietf_* functions.
 */

#pragma once

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "wfb_protocol.h"

#ifdef __cplusplus
extern "C" {
#endif

// ============================================================================
// Constants
// ============================================================================

#define WFB_CHACHA20_KEY_SIZE       32
#define WFB_CHACHA20_NONCE_SIZE     12  // IETF variant uses 12-byte nonce
#define WFB_POLY1305_TAG_SIZE       16

// For crypto_box (session key decryption)
#define WFB_CRYPTO_BOX_MACBYTES     16

// ============================================================================
// Types
// ============================================================================

/**
 * @brief Crypto context for WFB-NG decryption
 */
typedef struct {
    uint8_t session_key[WFB_CHACHA20_KEY_SIZE];     // Current session key
    uint64_t session_epoch;                          // Current session epoch
    uint32_t channel_id;                             // Expected channel ID
    uint8_t  fec_k;                                  // FEC data fragments
    uint8_t  fec_n;                                  // FEC total fragments
    bool     session_valid;                          // Session key loaded

    // Ground station keys for session key decryption
    uint8_t gs_public_key[WFB_CRYPTO_BOX_PUBLICKEYBYTES];
    uint8_t gs_secret_key[WFB_CRYPTO_BOX_SECRETKEYBYTES];
    bool    gs_keys_loaded;
} wfb_crypto_ctx_t;

// ============================================================================
// Initialization
// ============================================================================

/**
 * @brief Initialize crypto context
 *
 * @param ctx Crypto context to initialize
 */
void wfb_crypto_init(wfb_crypto_ctx_t* ctx);

/**
 * @brief Load ground station keypair for session key decryption
 *
 * @param ctx Crypto context
 * @param public_key 32-byte public key
 * @param secret_key 32-byte secret key
 * @return true on success
 */
bool wfb_crypto_load_keypair(wfb_crypto_ctx_t* ctx,
                              const uint8_t* public_key,
                              const uint8_t* secret_key);

/**
 * @brief Load keypair from gs.key file format
 *
 * The gs.key file contains 64 bytes: [secret_key(32)][public_key(32)]
 *
 * @param ctx Crypto context
 * @param key_data 64-byte key file data
 * @return true on success
 */
bool wfb_crypto_load_gs_key(wfb_crypto_ctx_t* ctx, const uint8_t* key_data);

// ============================================================================
// Session Key Handling
// ============================================================================

/**
 * @brief Process a session key packet
 *
 * Decrypts the session key packet and updates the crypto context
 * with the new session key and FEC parameters.
 *
 * @param ctx Crypto context
 * @param session_pkt Pointer to session packet (after IEEE header)
 * @param pkt_len Length of session packet
 * @param tx_public_key 32-byte transmitter public key
 * @return true if session key successfully decrypted and loaded
 */
bool wfb_crypto_process_session(wfb_crypto_ctx_t* ctx,
                                 const uint8_t* session_pkt,
                                 size_t pkt_len,
                                 const uint8_t* tx_public_key);

// ============================================================================
// Data Packet Decryption
// ============================================================================

/**
 * @brief Decrypt a WFB-NG data packet
 *
 * Decrypts the packet payload using ChaCha20-Poly1305 AEAD.
 *
 * @param ctx Crypto context with loaded session key
 * @param ciphertext Encrypted data (after wfb_block_hdr_t)
 * @param ciphertext_len Length of encrypted data (includes 16-byte auth tag)
 * @param nonce 8-byte nonce from wfb_block_hdr_t.data_nonce
 * @param plaintext Output buffer for decrypted data
 * @param plaintext_len Output: length of decrypted data
 * @return true if decryption and authentication successful
 */
bool wfb_crypto_decrypt(wfb_crypto_ctx_t* ctx,
                         const uint8_t* ciphertext,
                         size_t ciphertext_len,
                         uint64_t nonce,
                         uint8_t* plaintext,
                         size_t* plaintext_len);

// ============================================================================
// Low-level Crypto Functions (ChaCha20-Poly1305)
// ============================================================================

/**
 * @brief ChaCha20-Poly1305 AEAD decryption (IETF variant)
 *
 * @param plaintext Output buffer
 * @param plaintext_len Output: decrypted length
 * @param ciphertext Input ciphertext + 16-byte tag
 * @param ciphertext_len Length including tag
 * @param aad Additional authenticated data (can be NULL)
 * @param aad_len Length of AAD
 * @param nonce 12-byte nonce
 * @param key 32-byte key
 * @return 0 on success, -1 on auth failure
 */
int wfb_chacha20poly1305_decrypt(uint8_t* plaintext,
                                  size_t* plaintext_len,
                                  const uint8_t* ciphertext,
                                  size_t ciphertext_len,
                                  const uint8_t* aad,
                                  size_t aad_len,
                                  const uint8_t* nonce,
                                  const uint8_t* key);

/**
 * @brief Crypto_box_open (NaCl-style public key decryption)
 *
 * Used for decrypting session key packets.
 *
 * @param plaintext Output buffer
 * @param ciphertext Input (nonce + ciphertext + tag)
 * @param ciphertext_len Total length
 * @param nonce 24-byte nonce
 * @param sender_pk Sender's public key
 * @param receiver_sk Receiver's secret key
 * @return 0 on success, -1 on failure
 */
int wfb_crypto_box_open(uint8_t* plaintext,
                         const uint8_t* ciphertext,
                         size_t ciphertext_len,
                         const uint8_t* nonce,
                         const uint8_t* sender_pk,
                         const uint8_t* receiver_sk);

#ifdef __cplusplus
}
#endif
