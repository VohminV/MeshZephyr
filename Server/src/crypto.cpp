#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "mbedtls/sha256.h"
#include "mbedtls/aes.h"
#include "mbedtls/gcm.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include "config.h"
#include "crypto.h"
#include "packet.h"

static const char* TAG = TAG_CRYPTO;

// ============================================================================
// ГЛОБАЛЬНЫЕ ПЕРЕМЕННЫЕ
// ============================================================================
static CryptoIdentity_t g_identity;
static SessionContext_t g_sessions[20];  // Уменьшено до разумного значения
static uint8_t g_session_count = 0;
static bool g_initialized = false;
static mbedtls_entropy_context g_entropy;
static mbedtls_ctr_drbg_context g_ctr_drbg;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ КРИПТОГРАФИИ
// ============================================================================
Status_t crypto_init(void)
{
    if (g_initialized) {
        return STATUS_OK;
    }
    
    ESP_LOGI(TAG, "Initializing cryptographic subsystem...");
    
    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_ctr_drbg);
    
    const char* personalization = "ESP32_BBS_CRYPTO";
    int ret = mbedtls_ctr_drbg_seed(&g_ctr_drbg,
                                    mbedtls_entropy_func,
                                    &g_entropy,
                                    (const unsigned char*)personalization,
                                    strlen(personalization));
    
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to seed DRBG: -0x%04X", -ret);
        return STATUS_CRYPTO_FAIL;
    }
    
    memset(&g_identity, 0, sizeof(g_identity));
    
    if (crypto_load_identity(&g_identity) != STATUS_OK) {
        ESP_LOGI(TAG, "No identity found, generating new one...");
        if (crypto_generate_identity(&g_identity) != STATUS_OK) {
            ESP_LOGE(TAG, "Failed to generate identity");
            return STATUS_CRYPTO_FAIL;
        }
        crypto_save_identity(&g_identity);
    }
    
    memset(g_sessions, 0, sizeof(g_sessions));
    g_session_count = 0;
    
    g_initialized = true;
    ESP_LOGI(TAG, "Cryptographic subsystem initialized");
    
    return STATUS_OK;
}

// ============================================================================
// ГЕНЕРАЦИЯ ИДЕНТИЧНОСТИ
// ============================================================================
Status_t crypto_generate_identity(CryptoIdentity_t* identity)
{
    if (!identity) {
        return STATUS_INVALID;
    }
    
    ESP_LOGI(TAG, "Generating new cryptographic identity...");
    
    int ret = crypto_random_bytes(identity->private_key, KEY_SIZE);
    if (ret != STATUS_OK) {
        return STATUS_CRYPTO_FAIL;
    }
    
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);
    mbedtls_sha256_update(&sha256_ctx, identity->private_key, KEY_SIZE);
    mbedtls_sha256_finish(&sha256_ctx, identity->public_key);
    mbedtls_sha256_free(&sha256_ctx);
    
    ret = crypto_random_bytes(identity->sign_private, KEY_SIZE);
    if (ret != STATUS_OK) {
        return STATUS_CRYPTO_FAIL;
    }
    
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);
    mbedtls_sha256_update(&sha256_ctx, identity->sign_private, KEY_SIZE);
    mbedtls_sha256_finish(&sha256_ctx, identity->sign_public);
    mbedtls_sha256_free(&sha256_ctx);
    
    identity->initialized = true;
    identity->created_at = esp_timer_get_time() / 1000;
    
    ESP_LOGI(TAG, "Identity generated successfully");
    
    return STATUS_OK;
}

// ============================================================================
// ЗАГРУЗКА ИДЕНТИЧНОСТИ
// ============================================================================
Status_t crypto_load_identity(CryptoIdentity_t* identity)
{
    if (!identity) {
        return STATUS_INVALID;
    }
    
    char path[64];
    snprintf(path, sizeof(path), "%s/identity.dat", KEYS_DIR);
    
    FILE* f = fopen(path, "rb");
    if (!f) {
        return STATUS_NOT_FOUND;
    }
    
    size_t read = fread(identity, 1, sizeof(CryptoIdentity_t), f);
    fclose(f);
    
    if (read != sizeof(CryptoIdentity_t)) {
        return STATUS_INVALID;
    }
    
    if (!identity->initialized) {
        return STATUS_INVALID;
    }
    
    ESP_LOGI(TAG, "Identity loaded from filesystem");
    
    return STATUS_OK;
}

// ============================================================================
// СОХРАНЕНИЕ ИДЕНТИЧНОСТИ
// ============================================================================
Status_t crypto_save_identity(const CryptoIdentity_t* identity)
{
    if (!identity) {
        return STATUS_INVALID;
    }
    
    char path[64];
    snprintf(path, sizeof(path), "%s/identity.dat", KEYS_DIR);
    
    FILE* f = fopen(path, "wb");
    if (!f) {
        return STATUS_FS_ERROR;
    }
    
    size_t written = fwrite(identity, 1, sizeof(CryptoIdentity_t), f);
    fclose(f);
    
    if (written != sizeof(CryptoIdentity_t)) {
        return STATUS_FS_ERROR;
    }
    
    ESP_LOGI(TAG, "Identity saved to filesystem");
    
    return STATUS_OK;
}

// ============================================================================
// ПОЛУЧЕНИЕ ИДЕНТИЧНОСТИ
// ============================================================================
const CryptoIdentity_t* crypto_get_identity(void)
{
    return &g_identity;
}

// ============================================================================
// СЛУЧАЙНЫЕ ЧИСЛА
// ============================================================================
Status_t crypto_random_bytes(uint8_t* buffer, size_t len)
{
    if (!buffer) {
        return STATUS_INVALID;
    }
    
    int ret = mbedtls_ctr_drbg_random(&g_ctr_drbg, buffer, len);
    if (ret != 0) {
        return STATUS_CRYPTO_FAIL;
    }
    
    return STATUS_OK;
}

uint32_t crypto_random_uint32(void)
{
    uint32_t value;
    crypto_random_bytes((uint8_t*)&value, sizeof(value));
    return value;
}

// ============================================================================
// SHA-256 ХЭШ
// ============================================================================
Status_t crypto_sha256(const uint8_t* data, size_t len, uint8_t* hash)
{
    if (!data || !hash) {
        return STATUS_INVALID;
    }
    
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);
    mbedtls_sha256_update(&sha256_ctx, data, len);
    mbedtls_sha256_finish(&sha256_ctx, hash);
    mbedtls_sha256_free(&sha256_ctx);
    
    return STATUS_OK;
}

// ============================================================================
// HMAC-SHA256
// ============================================================================
Status_t crypto_hmac_sha256(
    const uint8_t* key,
    size_t key_len,
    const uint8_t* data,
    size_t data_len,
    uint8_t* hmac
)
{
    if (!key || !data || !hmac) {
        return STATUS_INVALID;
    }
    
    mbedtls_md_context_t ctx;
    const mbedtls_md_info_t* md_info = mbedtls_md_info_from_type(MBEDTLS_MD_SHA256);
    
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, md_info, 1);
    mbedtls_md_hmac_starts(&ctx, key, key_len);
    mbedtls_md_hmac_update(&ctx, data, data_len);
    mbedtls_md_hmac_finish(&ctx, hmac);
    mbedtls_md_free(&ctx);
    
    return STATUS_OK;
}

// ============================================================================
// ВЫЧИСЛЕНИЕ ОБЩЕГО СЕКРЕТА
// ============================================================================
Status_t crypto_compute_shared_secret(
    const uint8_t* private_key,
    const uint8_t* peer_public_key,
    uint8_t* shared_secret
)
{
    if (!private_key || !peer_public_key || !shared_secret) {
        return STATUS_INVALID;
    }
    
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);
    mbedtls_sha256_update(&sha256_ctx, private_key, KEY_SIZE);
    mbedtls_sha256_update(&sha256_ctx, peer_public_key, KEY_SIZE);
    mbedtls_sha256_finish(&sha256_ctx, shared_secret);
    mbedtls_sha256_free(&sha256_ctx);
    
    return STATUS_OK;
}

// ============================================================================
// ВЫВОД КЛЮЧА СЕССИИ
// ============================================================================
Status_t crypto_derive_session_key(
    const uint8_t* shared_secret,
    const uint8_t* salt,
    uint8_t* session_key
)
{
    if (!shared_secret || !salt || !session_key) {
        return STATUS_INVALID;
    }
    
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);
    mbedtls_sha256_update(&sha256_ctx, shared_secret, KEY_SIZE);
    mbedtls_sha256_update(&sha256_ctx, salt, 16);
    mbedtls_sha256_finish(&sha256_ctx, session_key);
    mbedtls_sha256_free(&sha256_ctx);
    
    return STATUS_OK;
}

// ============================================================================
// ПОДПИСЬ СООБЩЕНИЯ
// ============================================================================
Status_t crypto_sign_message(
    const uint8_t* private_key,
    const uint8_t* message,
    size_t message_len,
    uint8_t* signature
)
{
    if (!private_key || !message || !signature) {
        return STATUS_INVALID;
    }
    
    uint8_t hash[32];
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);
    mbedtls_sha256_update(&sha256_ctx, message, message_len);
    mbedtls_sha256_finish(&sha256_ctx, hash);
    mbedtls_sha256_free(&sha256_ctx);
    
    crypto_hmac_sha256(private_key, KEY_SIZE, hash, 32, signature);
    
    return STATUS_OK;
}

// ============================================================================
// ПРОВЕРКА ПОДПИСИ
// ============================================================================
Status_t crypto_verify_signature(
    const uint8_t* public_key,
    const uint8_t* message,
    size_t message_len,
    const uint8_t* signature
)
{
    if (!public_key || !message || !signature) {
        return STATUS_INVALID;
    }
    
    uint8_t hash[32];
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);
    mbedtls_sha256_update(&sha256_ctx, message, message_len);
    mbedtls_sha256_finish(&sha256_ctx, hash);
    mbedtls_sha256_free(&sha256_ctx);
    
    uint8_t expected_sig[SIGNATURE_SIZE];
    crypto_hmac_sha256(public_key, KEY_SIZE, hash, 32, expected_sig);
    
    if (memcmp(signature, expected_sig, SIGNATURE_SIZE) != 0) {
        return STATUS_CRYPTO_FAIL;
    }
    
    return STATUS_OK;
}

// ============================================================================
// ШИФРОВАНИЕ AES-GCM
// ============================================================================
Status_t crypto_encrypt_aes_gcm(
    const uint8_t* key,
    const uint8_t* nonce,
    const uint8_t* plaintext,
    size_t plaintext_len,
    uint8_t* ciphertext,
    uint8_t* tag
)
{
    if (!key || !nonce || !plaintext || !ciphertext || !tag) {
        return STATUS_INVALID;
    }
    
    mbedtls_gcm_context gcm_ctx;
    mbedtls_gcm_init(&gcm_ctx);
    
    int ret = mbedtls_gcm_setkey(&gcm_ctx,
                                  MBEDTLS_CIPHER_ID_AES,
                                  key,
                                  AES_KEY_SIZE * 8);
    
    if (ret != 0) {
        mbedtls_gcm_free(&gcm_ctx);
        return STATUS_CRYPTO_FAIL;
    }
    
    ret = mbedtls_gcm_crypt_and_tag(&gcm_ctx,
                                     MBEDTLS_GCM_ENCRYPT,
                                     plaintext_len,
                                     nonce,
                                     AES_IV_SIZE,
                                     NULL,
                                     0,
                                     plaintext,
                                     ciphertext,
                                     AES_TAG_SIZE,
                                     tag);
    
    mbedtls_gcm_free(&gcm_ctx);
    
    if (ret != 0) {
        return STATUS_CRYPTO_FAIL;
    }
    
    return STATUS_OK;
}

// ============================================================================
// РАСШИФРОВАНИЕ AES-GCM
// ============================================================================
Status_t crypto_decrypt_aes_gcm(
    const uint8_t* key,
    const uint8_t* nonce,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const uint8_t* tag,
    uint8_t* plaintext
)
{
    if (!key || !nonce || !ciphertext || !tag || !plaintext) {
        return STATUS_INVALID;
    }
    
    mbedtls_gcm_context gcm_ctx;
    mbedtls_gcm_init(&gcm_ctx);
    
    int ret = mbedtls_gcm_setkey(&gcm_ctx,
                                  MBEDTLS_CIPHER_ID_AES,
                                  key,
                                  AES_KEY_SIZE * 8);
    
    if (ret != 0) {
        mbedtls_gcm_free(&gcm_ctx);
        return STATUS_CRYPTO_FAIL;
    }
    
    ret = mbedtls_gcm_auth_decrypt(&gcm_ctx,
                                    ciphertext_len,
                                    nonce,
                                    AES_IV_SIZE,
                                    NULL,
                                    0,
                                    tag,
                                    AES_TAG_SIZE,
                                    ciphertext,
                                    plaintext);
    
    mbedtls_gcm_free(&gcm_ctx);
    
    if (ret != 0) {
        return STATUS_CRYPTO_FAIL;
    }
    
    return STATUS_OK;
}

// ============================================================================
// СЕССИИ
// ============================================================================
Status_t crypto_create_session(uint16_t peer_id, SessionContext_t* session)
{
    if (!session) {
        return STATUS_INVALID;
    }
    
    SessionContext_t* existing = crypto_find_session(peer_id);
    if (existing) {
        crypto_refresh_session(existing);
        memcpy(session, existing, sizeof(SessionContext_t));
        return STATUS_OK;
    }
    
    if (g_session_count >= 20) {
        crypto_cleanup_sessions();
    }
    
    memset(session, 0, sizeof(SessionContext_t));
    session->peer_id = peer_id;
    session->active = true;
    session->created_at = esp_timer_get_time() / 1000;
    session->expires_at = session->created_at + SESSION_KEY_EXPIRY;
    
    crypto_random_bytes(session->session_key, AES_KEY_SIZE);
    crypto_random_bytes(session->nonce, AES_IV_SIZE);
    
    if (peer_id < 20) {
        g_sessions[peer_id] = *session;
        g_session_count++;
    }
    
    ESP_LOGI(TAG, "Session created for peer %d", peer_id);
    
    return STATUS_OK;
}

SessionContext_t* crypto_find_session(uint16_t peer_id)
{
    if (peer_id >= 20) {
        return NULL;
    }
    
    if (!g_sessions[peer_id].active) {
        return NULL;
    }
    
    uint32_t now = esp_timer_get_time() / 1000;
    if (now > g_sessions[peer_id].expires_at) {
        crypto_close_session(peer_id);
        return NULL;
    }
    
    return &g_sessions[peer_id];
}

Status_t crypto_refresh_session(SessionContext_t* session)
{
    if (!session) {
        return STATUS_INVALID;
    }
    
    session->expires_at = esp_timer_get_time() / 1000 + SESSION_KEY_EXPIRY;
    crypto_random_bytes(session->nonce, AES_IV_SIZE);
    
    return STATUS_OK;
}

Status_t crypto_close_session(uint16_t peer_id)
{
    if (peer_id >= 20) {
        return STATUS_INVALID;
    }
    
    if (g_sessions[peer_id].active) {
        memset(&g_sessions[peer_id], 0, sizeof(SessionContext_t));
        if (g_session_count > 0) g_session_count--;
        ESP_LOGI(TAG, "Session closed for peer %d", peer_id);
    }
    
    return STATUS_OK;
}

void crypto_cleanup_sessions(void)
{
    uint32_t now = esp_timer_get_time() / 1000;
    
    for (uint16_t i = 0; i < 20; i++) {
        if (g_sessions[i].active && now > g_sessions[i].expires_at) {
            crypto_close_session(i);
        }
    }
}