#include "wmbus_keyring.h"

#include <ctype.h>
#include <stdio.h>
#include <string.h>

#include <toolbox/args.h>
#include <toolbox/stream/file_stream.h>
#include <toolbox/stream/stream.h>

#include "../protocol/format/wmbus_hex_utils.h"

#define WMBUS_KEYRING_LINE_MAX (WMBUS_KEY_BYTES * 2U + 2U)

static void wmbus_keyring_set_status(WmBusKeyring* keyring, const char* format, ...) {
    if(!keyring || !format) return;

    va_list args;
    va_start(args, format);
    vsnprintf(keyring->status, sizeof(keyring->status), format, args);
    va_end(args);
}

static void wmbus_keyring_trim(char* str) {
    if(!str) return;

    size_t len = strlen(str);
    while(len > 0U && isspace((unsigned char)str[len - 1U])) {
        str[--len] = '\0';
    }

    size_t start = 0;
    while(str[start] != '\0' && isspace((unsigned char)str[start])) {
        start++;
    }

    if(start > 0U) {
        memmove(str, &str[start], strlen(&str[start]) + 1U);
    }
}

static void wmbus_keyring_uppercase(char* str) {
    if(!str) return;
    for(size_t i = 0; str[i] != '\0'; i++) {
        str[i] = (char)toupper((unsigned char)str[i]);
    }
}

static bool wmbus_keyring_parse_hex_key(const char* text, uint8_t out[WMBUS_KEY_BYTES]) {
    if(!text || !out) return false;
    if(strlen(text) != (WMBUS_KEY_BYTES * 2U)) return false;

    for(size_t i = 0; i < WMBUS_KEY_BYTES; i++) {
        if(!args_char_to_hex(text[i * 2U], text[i * 2U + 1U], &out[i])) {
            return false;
        }
    }

    return true;
}

static void wmbus_keyring_format_hex_key(
    const uint8_t key[WMBUS_KEY_BYTES],
    char out[WMBUS_KEYRING_LINE_MAX]) {
    if(!key || !out) return;

    wmbus_hex_encode(key, WMBUS_KEY_BYTES, out, WMBUS_KEYRING_LINE_MAX - 1U);
    out[WMBUS_KEY_BYTES * 2U] = '\n';
    out[WMBUS_KEY_BYTES * 2U + 1U] = '\0';
}

void wmbus_keyring_init(WmBusKeyring* keyring) {
    if(!keyring) return;

    memset(keyring, 0, sizeof(*keyring));
    snprintf(keyring->path, sizeof(keyring->path), "%s", WMBUS_KEYRING_PATH);
    wmbus_keyring_set_status(keyring, "missing");
}

void wmbus_keyring_copy_key_store(const WmBusKeyring* keyring, WmBusCryptoKeyStore* out_key_store) {
    if(!out_key_store) {
        return;
    }

    memset(out_key_store, 0, sizeof(*out_key_store));
    if(!keyring) {
        return;
    }

    out_key_store->count = keyring->count;
    for(uint8_t i = 0; i < keyring->count; i++) {
        memcpy(out_key_store->keys[i], keyring->keys[i], WMBUS_KEY_BYTES);
    }
}

bool wmbus_keyring_load(Storage* storage, WmBusKeyring* keyring) {
    if(!storage || !keyring) return false;

    WmBusKeyring loaded;
    wmbus_keyring_init(&loaded);
    snprintf(loaded.path, sizeof(loaded.path), "%s", keyring->path);
    wmbus_storage_ensure_app_folder(storage);

    Stream* stream = file_stream_alloc(storage);
    FuriString* line = furi_string_alloc();
    bool ok = true;

    if(!file_stream_open(stream, loaded.path, FSAM_READ, FSOM_OPEN_EXISTING)) {
        loaded.file_present = false;
        loaded.loaded = false;
        wmbus_keyring_set_status(&loaded, "missing");
        *keyring = loaded;
        furi_string_free(line);
        file_stream_close(stream);
        stream_free(stream);
        return false;
    }

    loaded.file_present = true;

    while(stream_read_line(stream, line)) {
        char buffer[128];
        snprintf(buffer, sizeof(buffer), "%s", furi_string_get_cstr(line));
        wmbus_keyring_trim(buffer);
        wmbus_keyring_uppercase(buffer);

        if((buffer[0] == '\0') || (buffer[0] == '#')) {
            continue;
        }

        if(loaded.count >= WMBUS_KEYRING_MAX_KEYS) {
            ok = false;
            wmbus_keyring_set_status(&loaded, "too many keys");
            break;
        }

        if(strchr(buffer, ',')) {
            ok = false;
            wmbus_keyring_set_status(&loaded, "bad key");
            break;
        }

        if(!wmbus_keyring_parse_hex_key(buffer, loaded.keys[loaded.count])) {
            ok = false;
            wmbus_keyring_set_status(&loaded, "bad key");
            break;
        }

        loaded.count++;
    }

    if(ok) {
        loaded.loaded = true;
        wmbus_keyring_set_status(&loaded, "%u keys", (unsigned int)loaded.count);
    } else {
        loaded.loaded = false;
        loaded.count = 0;
    }

    *keyring = loaded;

    furi_string_free(line);
    file_stream_close(stream);
    stream_free(stream);
    return ok;
}

bool wmbus_keyring_append(
    Storage* storage,
    WmBusKeyring* keyring,
    const uint8_t key[WMBUS_KEY_BYTES]) {
    if(!storage || !keyring || !key) return false;

    if(keyring->file_present && !keyring->loaded) {
        return false;
    }

    if(keyring->count >= WMBUS_KEYRING_MAX_KEYS) {
        wmbus_keyring_set_status(keyring, "too many keys");
        return false;
    }

    if(!wmbus_storage_ensure_app_folder(storage)) {
        wmbus_keyring_set_status(keyring, "storage err");
        return false;
    }

    char line[WMBUS_KEYRING_LINE_MAX];
    wmbus_keyring_format_hex_key(key, line);

    File* file = storage_file_alloc(storage);
    bool written = false;

    if(storage_file_open(file, keyring->path, FSAM_WRITE, FSOM_OPEN_APPEND)) {
        size_t line_len = strlen(line);
        written = storage_file_write(file, line, line_len) == line_len;
        if(written) {
            storage_file_sync(file);
        }
    }

    if(storage_file_is_open(file)) {
        storage_file_close(file);
    }
    storage_file_free(file);

    if(!written) {
        wmbus_keyring_set_status(keyring, "write fail");
        return false;
    }

    return wmbus_keyring_load(storage, keyring);
}
