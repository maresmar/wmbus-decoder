#pragma once

#include <stdbool.h>
#include <stdint.h>

#include <storage/storage.h>

#include "../protocol/crypto/wmbus_crypto_key_store.h"
#include "wmbus_paths.h"

#define WMBUS_KEY_BYTES        WMBUS_CRYPTO_KEY_BYTES
#define WMBUS_KEYRING_MAX_KEYS WMBUS_CRYPTO_KEY_STORE_MAX

typedef struct {
    uint8_t key[WMBUS_KEY_BYTES];
} WmBusKeyEntry;

typedef struct {
    char path[64];
    uint8_t count;
    bool file_present;
    bool loaded;
    char status[24];
    WmBusKeyEntry entries[WMBUS_KEYRING_MAX_KEYS];
} WmBusKeyring;

void wmbus_keyring_init(WmBusKeyring* keyring);
bool wmbus_keyring_load(Storage* storage, WmBusKeyring* keyring);
bool wmbus_keyring_append(
    Storage* storage,
    WmBusKeyring* keyring,
    const uint8_t key[WMBUS_KEY_BYTES]);
const WmBusKeyEntry* wmbus_keyring_get(const WmBusKeyring* keyring, uint8_t index);
void wmbus_keyring_copy_key_store(
    const WmBusKeyring* keyring,
    WmBusCryptoKeyStore* out_key_store);
