#pragma once

#include "config.h"
#include <stdint.h>
#include <stdbool.h>

// ============================================================================
// КРИПТОГРАФИЧЕСКИЕ КОНТЕКСТЫ
// ============================================================================
typedef struct {
    uint8_t  private_key[KEY_SIZE];       // X25519 закрытый ключ
    uint8_t  public_key[KEY_SIZE];        // X25519 открытый ключ
    uint8_t  sign_private[KEY_SIZE];      // Ed25519 закрытый ключ
    uint8_t  sign_public[KEY_SIZE];       // Ed25519 открытый ключ
    bool     initialized;
    uint32_t created_at;
} CryptoIdentity_t;

typedef struct {
    uint8_t  session_key[AES_KEY_SIZE];   // AES-128 ключ сессии
    uint8_t  nonce[AES_IV_SIZE];          // Nonce для GCM
    uint32_t created_at;
    uint32_t expires_at;
    uint16_t peer_id;
    bool     active;
} SessionContext_t;

// ============================================================================
// ИНИЦИАЛИЗАЦИЯ
// ============================================================================

#ifdef __cplusplus
extern "C" {
#endif

// Инициализация криптографической подсистемы
Status_t crypto_init(void);

// Генерация новой идентичности узла
Status_t crypto_generate_identity(CryptoIdentity_t* identity);

// Загрузка идентичности из файловой системы
Status_t crypto_load_identity(CryptoIdentity_t* identity);

// Сохранение идентичности в файловую систему
Status_t crypto_save_identity(const CryptoIdentity_t* identity);

// Получение глобальной идентичности узла
const CryptoIdentity_t* crypto_get_identity(void);

// ============================================================================
// ОБМЕН КЛЮЧАМИ (X25519)
// ============================================================================

// Вычисление общего секрета
Status_t crypto_compute_shared_secret(
    const uint8_t* private_key,
    const uint8_t* peer_public_key,
    uint8_t* shared_secret
);

// Derive AES ключ из общего секрета
Status_t crypto_derive_session_key(
    const uint8_t* shared_secret,
    const uint8_t* salt,
    uint8_t* session_key
);

// ============================================================================
// ЦИФРОВЫЕ ПОДПИСИ (Ed25519)
// ============================================================================

// Создание подписи
Status_t crypto_sign_message(
    const uint8_t* private_key,
    const uint8_t* message,
    size_t message_len,
    uint8_t* signature
);

// Проверка подписи
Status_t crypto_verify_signature(
    const uint8_t* public_key,
    const uint8_t* message,
    size_t message_len,
    const uint8_t* signature
);

// ============================================================================
// СИММЕТРИЧНОЕ ШИФРОВАНИЕ (AES-128-GCM)
// ============================================================================

// Шифрование с аутентификацией
Status_t crypto_encrypt_aes_gcm(
    const uint8_t* key,
    const uint8_t* nonce,
    const uint8_t* plaintext,
    size_t plaintext_len,
    uint8_t* ciphertext,
    uint8_t* tag
);

// Расшифрование с аутентификацией
Status_t crypto_decrypt_aes_gcm(
    const uint8_t* key,
    const uint8_t* nonce,
    const uint8_t* ciphertext,
    size_t ciphertext_len,
    const uint8_t* tag,
    uint8_t* plaintext
);

// ============================================================================
// СЕССИИ
// ============================================================================

// Создание новой сессии
Status_t crypto_create_session(uint16_t peer_id, SessionContext_t* session);

// Поиск активной сессии
SessionContext_t* crypto_find_session(uint16_t peer_id);

// Обновление сессии
Status_t crypto_refresh_session(SessionContext_t* session);

// Удаление сессии
Status_t crypto_close_session(uint16_t peer_id);

// Очистка всех сессий
void crypto_cleanup_sessions(void);

// ============================================================================
// ХЭШИРОВАНИЕ
// ============================================================================

// SHA-256 хэш
Status_t crypto_sha256(const uint8_t* data, size_t len, uint8_t* hash);

// HMAC-SHA256
Status_t crypto_hmac_sha256(
    const uint8_t* key, size_t key_len,
    const uint8_t* data, size_t data_len,
    uint8_t* hmac
);

// ============================================================================
// СЛУЧАЙНЫЕ ЧИСЛА
// ============================================================================

// Генерация криптографически стойких случайных данных
Status_t crypto_random_bytes(uint8_t* buffer, size_t len);

// Генерация случайного числа
uint32_t crypto_random_uint32(void);

#ifdef __cplusplus
}
#endif