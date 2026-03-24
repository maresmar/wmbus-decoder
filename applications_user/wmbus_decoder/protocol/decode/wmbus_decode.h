#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool wmbus_decode_3of6_bits(
    const uint8_t* raw,
    size_t raw_bit_len,
    uint8_t bit_offset,
    uint8_t* out,
    size_t out_max,
    size_t* out_len);

bool wmbus_decode_3of6(
    const uint8_t* raw,
    size_t raw_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len);

bool wmbus_decode_is_plausible_frame(const uint8_t* data, size_t len);
