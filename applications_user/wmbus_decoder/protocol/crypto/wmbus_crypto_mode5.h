#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../../core/wmbus_types.h"

WmBusMode5DecryptInfo wmbus_crypto_mode5_decrypt(
    const uint8_t* frame,
    size_t frame_len,
    uint16_t cfg,
    const uint8_t key[16],
    uint8_t* out_frame);
