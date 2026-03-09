#include "wmbus_parser.h"

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

static uint8_t wmbus_get_bits_msb(const uint8_t* data, size_t bit_pos, size_t bit_count) {
    uint8_t out = 0;
    for(size_t i = 0; i < bit_count; i++) {
        size_t pos = bit_pos + i;
        uint8_t byte = data[pos / 8U];
        uint8_t bit = 7U - (pos % 8U);
        out = (out << 1) | ((byte >> bit) & 0x01U);
    }
    return out;
}

bool wmbus_parser_decode_3of6_bits(
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
        uint8_t sym = wmbus_get_bits_msb(raw, bit_pos, 6);
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
    return (out_idx > 0) && !have_high;
}

bool wmbus_parser_decode_3of6(
    const uint8_t* raw,
    size_t raw_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len) {
    return wmbus_parser_decode_3of6_bits(raw, raw_len * 8U, 0U, out, out_max, out_len);
}

static bool wmbus_mfg_valid(uint16_t man) {
    uint8_t a = (man >> 10) & 0x1F;
    uint8_t b = (man >> 5) & 0x1F;
    uint8_t c = man & 0x1F;
    return (a >= 1 && a <= 26) && (b >= 1 && b <= 26) && (c >= 1 && c <= 26);
}

static bool wmbus_c_field_valid(uint8_t c_field) {
    return c_field == 0x44 || c_field == 0x46;
}

bool wmbus_parser_is_plausible(const uint8_t* data, size_t len) {
    if(len < 11) return false;
    uint8_t l_field = data[0];
    if(l_field < 10) return false;
    if(!wmbus_c_field_valid(data[1])) return false;
    uint16_t man = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
    if(!wmbus_mfg_valid(man)) return false;
    return true;
}

int wmbus_parser_apator162_register_size(uint8_t reg) {
    switch(reg) {
    case 0x00:
        return 4;
    case 0x01:
        return 3;
    case 0xA1:
    case 0x10:
        return 4;
    case 0x11:
        return 2;
    case 0x40:
        return 6;
    case 0x41:
        return 2;
    case 0x42:
        return 4;
    case 0x43:
        return 2;
    case 0x44:
        return 3;
    case 0x71:
        return 1 + 2 * 4;
    case 0x72:
        return 1 + 3 * 4;
    case 0x73:
        return 1 + 4 * 4;
    case 0x74:
        return 1 + 5 * 4;
    case 0x75:
        return 1 + 6 * 4;
    case 0x76:
        return 1 + 7 * 4;
    case 0x77:
        return 1 + 8 * 4;
    case 0x78:
        return 1 + 9 * 4;
    case 0x79:
        return 1 + 10 * 4;
    case 0x7A:
        return 1 + 11 * 4;
    case 0x7B:
        return 1 + 12 * 4;
    case 0x80:
    case 0x81:
    case 0x82:
    case 0x83:
    case 0x84:
    case 0x86:
    case 0x87:
        return 10;
    case 0x85:
    case 0x88:
    case 0x8F:
        return 11;
    case 0x8A:
        return 9;
    case 0x8B:
    case 0x8C:
        return 6;
    case 0x8E:
        return 7;
    case 0xA0:
        return 4;
    case 0xA2:
        return 1;
    case 0xA3:
        return 7;
    case 0xA4:
        return 4;
    case 0xA5:
    case 0xA9:
    case 0xAF:
        return 1;
    case 0xA6:
        return 3;
    case 0xA7:
    case 0xA8:
    case 0xAA:
    case 0xAB:
    case 0xAC:
    case 0xAD:
        return 2;
    case 0xB0:
        return 5;
    case 0xB1:
        return 8;
    case 0xB2:
        return 16;
    case 0xB3:
        return 8;
    case 0xB4:
        return 2;
    case 0xB5:
        return 16;
    case 0xB6:
    case 0xB7:
    case 0xB8:
    case 0xB9:
    case 0xBA:
    case 0xBB:
    case 0xBC:
    case 0xBD:
    case 0xBE:
    case 0xBF:
    case 0xC0:
    case 0xC1:
    case 0xC2:
    case 0xC3:
    case 0xC4:
    case 0xC5:
    case 0xC6:
    case 0xC7:
    case 0xD0:
    case 0xD3:
        return 3;
    case 0xF0:
        return 4;
    default:
        return -1;
    }
}

bool wmbus_parser_parse_apator162_total(
    const uint8_t* frame,
    size_t frame_len,
    uint32_t* total_m3_x1000) {
    if(frame_len < 15) return false;
    if(frame[10] != 0x7A) return false;

    size_t pos = 15;
    while(pos < frame_len && frame[pos] == 0x2F) {
        pos++;
    }

    if(pos + 8 > frame_len) return false;
    pos += 8;

    while(pos < frame_len) {
        uint8_t reg = frame[pos++];
        if(reg == 0xFF) break;

        int reg_size = wmbus_parser_apator162_register_size(reg);
        if(reg_size < 0) return false;
        if(pos + (size_t)reg_size > frame_len) return false;

        if((reg == 0x10 || reg == 0xA1) && reg_size >= 4) {
            *total_m3_x1000 = (uint32_t)frame[pos] | ((uint32_t)frame[pos + 1] << 8) |
                              ((uint32_t)frame[pos + 2] << 16) | ((uint32_t)frame[pos + 3] << 24);
            return true;
        }

        pos += (size_t)reg_size;
    }

    return false;
}
