#include "wmbus_decode.h"

static uint8_t wmbus_3of6_decode_symbol(uint8_t sym) {
    switch(sym) {
    case 0x16:
        return 0x0;
    case 0x0D:
        return 0x1;
    case 0x0E:
        return 0x2;
    case 0x0B:
        return 0x3;
    case 0x1C:
        return 0x4;
    case 0x19:
        return 0x5;
    case 0x1A:
        return 0x6;
    case 0x13:
        return 0x7;
    case 0x2C:
        return 0x8;
    case 0x25:
        return 0x9;
    case 0x26:
        return 0xA;
    case 0x23:
        return 0xB;
    case 0x34:
        return 0xC;
    case 0x31:
        return 0xD;
    case 0x32:
        return 0xE;
    case 0x29:
        return 0xF;
    default:
        return 0xFF;
    }
}

static uint8_t wmbus_decode_get_bits_msb(const uint8_t* data, size_t bit_pos, size_t bit_count) {
    uint8_t out = 0;
    for(size_t i = 0; i < bit_count; i++) {
        size_t pos = bit_pos + i;
        uint8_t byte = data[pos / 8U];
        uint8_t bit = 7U - (pos % 8U);
        out = (out << 1) | ((byte >> bit) & 0x01U);
    }
    return out;
}

bool wmbus_decode_3of6_bits(
    const uint8_t* raw,
    size_t raw_bit_len,
    uint8_t bit_offset,
    uint8_t* out,
    size_t out_max,
    size_t* out_len) {
    if(!raw || !out || !out_len || bit_offset > 7U) return false;
    if(raw_bit_len <= bit_offset) return false;

    size_t bit_pos = bit_offset;
    size_t out_idx = 0;
    bool have_high = false;
    uint8_t high_nibble = 0;

    while((bit_pos + 6U) <= raw_bit_len) {
        uint8_t sym = wmbus_decode_get_bits_msb(raw, bit_pos, 6);
        bit_pos += 6U;

        uint8_t nibble = wmbus_3of6_decode_symbol(sym);
        if(nibble == 0xFF) {
            return false;
        }

        if(!have_high) {
            high_nibble = nibble;
            have_high = true;
        } else {
            if(out_idx >= out_max) break;
            out[out_idx++] = (high_nibble << 4) | nibble;
            have_high = false;
        }
    }

    *out_len = out_idx;
    return (out_idx > 0U) && !have_high;
}

bool wmbus_decode_3of6(
    const uint8_t* raw,
    size_t raw_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len) {
    return wmbus_decode_3of6_bits(raw, raw_len * 8U, 0U, out, out_max, out_len);
}

static bool wmbus_decode_mfg_valid(uint16_t man) {
    uint8_t a = (man >> 10) & 0x1F;
    uint8_t b = (man >> 5) & 0x1F;
    uint8_t c = man & 0x1F;
    return (a >= 1U && a <= 26U) && (b >= 1U && b <= 26U) && (c >= 1U && c <= 26U);
}

static bool wmbus_decode_c_field_valid(uint8_t c_field) {
    return c_field == 0x44U || c_field == 0x46U;
}

bool wmbus_decode_is_plausible_frame(const uint8_t* data, size_t len) {
    if(!data || len < 11U) return false;

    uint8_t l_field = data[0];
    if(l_field < 10U) return false;
    if(!wmbus_decode_c_field_valid(data[1])) return false;

    uint16_t man = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
    return wmbus_decode_mfg_valid(man);
}
