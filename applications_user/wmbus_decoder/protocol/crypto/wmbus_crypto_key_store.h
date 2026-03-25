#pragma once

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

#define WMBUS_CRYPTO_KEY_BYTES     16U
#define WMBUS_CRYPTO_KEY_STORE_MAX 16U

typedef struct {
    uint8_t bytes[WMBUS_CRYPTO_KEY_BYTES];
} WmBusCryptoKey;

typedef struct {
    uint8_t count;
    WmBusCryptoKey entries[WMBUS_CRYPTO_KEY_STORE_MAX];
} WmBusCryptoKeyStore;

static inline const WmBusCryptoKey*
    wmbus_crypto_key_store_get(const WmBusCryptoKeyStore* key_store, uint8_t index) {
    if(!key_store || index >= key_store->count) {
        return NULL;
    }

    return &key_store->entries[index];
}
