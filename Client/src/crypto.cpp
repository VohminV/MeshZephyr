#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_timer.h"
#include "mbedtls/sha256.h"
#include "mbedtls/entropy.h"
#include "mbedtls/ctr_drbg.h"

#include "config.h"
#include "crypto.h"

static const char* TAG = TAG_CRYPTO;

static CryptoIdentity_t g_identity;
static bool g_initialized = false;
static mbedtls_entropy_context g_entropy;
static mbedtls_ctr_drbg_context g_ctr_drbg;

Status_t crypto_init(void)
{
    if (g_initialized) return STATUS_OK;
    
    ESP_LOGI(TAG, "Initializing crypto subsystem...");
    
    mbedtls_entropy_init(&g_entropy);
    mbedtls_ctr_drbg_init(&g_ctr_drbg);
    
    const char* personalization = "MESH_ZEPHYR_CLIENT";
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
        ESP_LOGI(TAG, "Generating new identity...");
        if (crypto_generate_identity(&g_identity) != STATUS_OK) {
            ESP_LOGE(TAG, "Failed to generate identity");
            return STATUS_CRYPTO_FAIL;
        }
        crypto_save_identity(&g_identity);
    }
    
    g_initialized = true;
    ESP_LOGI(TAG, "Crypto initialized");
    
    return STATUS_OK;
}

Status_t crypto_generate_identity(CryptoIdentity_t* identity)
{
    if (!identity) return STATUS_INVALID;
    
    if (crypto_random_bytes(identity->private_key, KEY_SIZE) != STATUS_OK) {
        return STATUS_CRYPTO_FAIL;
    }
    
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);
    mbedtls_sha256_update(&sha256_ctx, identity->private_key, KEY_SIZE);
    mbedtls_sha256_finish(&sha256_ctx, identity->public_key);
    mbedtls_sha256_free(&sha256_ctx);
    
    if (crypto_random_bytes(identity->sign_private, KEY_SIZE) != STATUS_OK) {
        return STATUS_CRYPTO_FAIL;
    }
    
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);
    mbedtls_sha256_update(&sha256_ctx, identity->sign_private, KEY_SIZE);
    mbedtls_sha256_finish(&sha256_ctx, identity->sign_public);
    mbedtls_sha256_free(&sha256_ctx);
    
    identity->initialized = true;
    identity->created_at = esp_timer_get_time() / 1000;
    
    return STATUS_OK;
}

Status_t crypto_load_identity(CryptoIdentity_t* identity)
{
    if (!identity) return STATUS_INVALID;
    
    char path[64];
    snprintf(path, sizeof(path), "/bbs/keys/identity.dat");
    
    FILE* f = fopen(path, "rb");
    if (!f) return STATUS_NOT_FOUND;
    
    size_t read = fread(identity, 1, sizeof(CryptoIdentity_t), f);
    fclose(f);
    
    if (read != sizeof(CryptoIdentity_t)) return STATUS_INVALID;
    if (!identity->initialized) return STATUS_INVALID;
    
    return STATUS_OK;
}

Status_t crypto_save_identity(const CryptoIdentity_t* identity)
{
    if (!identity) return STATUS_INVALID;
    
    char path[64];
    snprintf(path, sizeof(path), "/bbs/keys/identity.dat");
    
    FILE* f = fopen(path, "wb");
    if (!f) return STATUS_FS_ERROR;
    
    size_t written = fwrite(identity, 1, sizeof(CryptoIdentity_t), f);
    fclose(f);
    
    if (written != sizeof(CryptoIdentity_t)) return STATUS_FS_ERROR;
    
    return STATUS_OK;
}

const CryptoIdentity_t* crypto_get_identity(void)
{
    return &g_identity;
}

Status_t crypto_random_bytes(uint8_t* buffer, size_t len)
{
    if (!buffer) return STATUS_INVALID;
    
    int ret = mbedtls_ctr_drbg_random(&g_ctr_drbg, buffer, len);
    if (ret != 0) return STATUS_CRYPTO_FAIL;
    
    return STATUS_OK;
}

uint32_t crypto_random_uint32(void)
{
    uint32_t value;
    crypto_random_bytes((uint8_t*)&value, sizeof(value));
    return value;
}

Status_t crypto_sha256(const uint8_t* data, size_t len, uint8_t* hash)
{
    if (!data || !hash) return STATUS_INVALID;
    
    mbedtls_sha256_context sha256_ctx;
    mbedtls_sha256_init(&sha256_ctx);
    mbedtls_sha256_starts(&sha256_ctx, 0);
    mbedtls_sha256_update(&sha256_ctx, data, len);
    mbedtls_sha256_finish(&sha256_ctx, hash);
    mbedtls_sha256_free(&sha256_ctx);
    
    return STATUS_OK;
}

Status_t crypto_hmac_sha256(const uint8_t* key, size_t key_len,
                            const uint8_t* data, size_t data_len,
                            uint8_t* hmac)
{
    if (!key || !data || !hmac) return STATUS_INVALID;
    
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

Status_t crypto_sign_message(const uint8_t* private_key, 
                             const uint8_t* message,
                             size_t message_len, 
                             uint8_t* signature)
{
    if (!private_key || !message || !signature) return STATUS_INVALID;
    
    uint8_t hash[32];
    crypto_sha256(message, message_len, hash);
    crypto_hmac_sha256(private_key, KEY_SIZE, hash, 32, signature);
    
    return STATUS_OK;
}

Status_t crypto_verify_signature(const uint8_t* public_key,
                                 const uint8_t* message,
                                 size_t message_len,
                                 const uint8_t* signature)
{
    if (!public_key || !message || !signature) return STATUS_INVALID;
    
    uint8_t hash[32];
    crypto_sha256(message, message_len, hash);
    
    uint8_t expected[KEY_SIZE];
    crypto_hmac_sha256(public_key, KEY_SIZE, hash, 32, expected);
    
    if (memcmp(signature, expected, KEY_SIZE) != 0) {
        return STATUS_CRYPTO_FAIL;
    }
    
    return STATUS_OK;
}