#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool wmbus_parser_decode_3of6_bits(
    const uint8_t* raw,
    size_t raw_bit_len,
    uint8_t bit_offset,
    uint8_t* out,
    size_t out_max,
    size_t* out_len);

bool wmbus_parser_decode_3of6(
    const uint8_t* raw,
    size_t raw_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len);

bool wmbus_parser_is_plausible(const uint8_t* data, size_t len);

int wmbus_parser_apator162_register_size(uint8_t reg);

bool wmbus_parser_parse_apator162_total(
    const uint8_t* frame,
    size_t frame_len,
    uint32_t* total_m3_x1000);
