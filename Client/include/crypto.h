#pragma once

#include "config.h"
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// Криптографическая идентичность узла
typedef struct {
    uint8_t  private_key[KEY_SIZE];
    uint8_t  public_key[KEY_SIZE];
    uint8_t  sign_private[KEY_SIZE];
    uint8_t  sign_public[KEY_SIZE];
    bool     initialized;
    uint32_t created_at;
} CryptoIdentity_t;

// Инициализация криптоподсистемы
Status_t crypto_init(void);

// Генерация новой идентичности
Status_t crypto_generate_identity(CryptoIdentity_t* identity);

// Загрузка из FS
Status_t crypto_load_identity(CryptoIdentity_t* identity);

// Сохранение в FS
Status_t crypto_save_identity(const CryptoIdentity_t* identity);

// Получить текущую идентичность
const CryptoIdentity_t* crypto_get_identity(void);

// Подпись сообщения
Status_t crypto_sign_message(const uint8_t* private_key, 
                             const uint8_t* message,
                             size_t message_len, 
                             uint8_t* signature);

// Проверка подписи
Status_t crypto_verify_signature(const uint8_t* public_key,
                                 const uint8_t* message,
                                 size_t message_len,
                                 const uint8_t* signature);

// Генерация случайных байт
Status_t crypto_random_bytes(uint8_t* buffer, size_t len);

// Случайное число 32 бит
uint32_t crypto_random_uint32(void);

// SHA-256 хэш
Status_t crypto_sha256(const uint8_t* data, size_t len, uint8_t* hash);

// HMAC-SHA256
Status_t crypto_hmac_sha256(const uint8_t* key, size_t key_len,
                            const uint8_t* data, size_t data_len,
                            uint8_t* hmac);

#ifdef __cplusplus
}
#endif