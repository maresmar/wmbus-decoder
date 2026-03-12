// Source derived from https://github.com/kokke/tiny-AES128-C
// Public Domain / CC0 / Unlicense

#pragma once

#include <stdint.h>

void wmbus_aes128_cbc_decrypt_buffer(
    uint8_t* output,
    const uint8_t* input,
    uint32_t length,
    const uint8_t* key,
    const uint8_t* iv);
