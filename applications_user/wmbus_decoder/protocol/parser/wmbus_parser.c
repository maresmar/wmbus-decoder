#include "wmbus_parser.h"
#include "../crypto/wmbus_aes.h"

#include "core/wmbus_types.h"
#include <string.h>

#define WMBUS_PARSER_FRAME_MAX 256U
#define WMBUS_AES_BLOCK_LEN    16U
#define WMBUS_SHORT_TPL_POS    15U

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

const char* wmbus_parser_id_name(WmBusParserId parser_id) {
    switch(parser_id) {
    case WmBusParserIdRaw:
        return "Raw";
    case WmBusParserIdHeader:
        return "Header";
    case WmBusParserIdShortTpl:
        return "Short TPL";
    case WmBusParserIdDifVif:
        return "DifVif";
    case WmBusParserIdApator162:
        return "Apator162";
    case WmBusParserIdUnknown:
    default:
        return "Unknown";
    }
}

bool wmbus_parser_id_is_generic(WmBusParserId parser_id) {
    switch(parser_id) {
    case WmBusParserIdUnknown:
    case WmBusParserIdRaw:
    case WmBusParserIdHeader:
    case WmBusParserIdShortTpl:
    case WmBusParserIdDifVif:
        return true;
    case WmBusParserIdApator162:
        return false;
    default:
        return true;
    }
}

uint8_t wmbus_parser_short_tpl_security_mode(uint16_t cfg) {
    return (uint8_t)((cfg >> 8) & 0x1FU);
}

bool wmbus_parser_short_tpl_security_likely_encrypted(uint16_t cfg) {
    // OMS short-TPL security modes that imply encrypted content.
    switch(wmbus_parser_short_tpl_security_mode(cfg)) {
    case 0x05:
    case 0x07:
    case 0x08:
    case 0x09:
    case 0x0A:
        return true;
    default:
        return false;
    }
}

bool wmbus_parser_short_tpl_payload_has_check_bytes(const uint8_t* frame, size_t frame_len) {
    return frame_len >= (WMBUS_SHORT_TPL_POS + 2U) && frame[WMBUS_SHORT_TPL_POS] == 0x2F &&
           frame[WMBUS_SHORT_TPL_POS + 1U] == 0x2F;
}

static void wmbus_parser_build_mode5_iv(const uint8_t* frame, uint8_t iv[WMBUS_AES_BLOCK_LEN]) {
    memcpy(iv, &frame[2], 8U);
    memset(&iv[8], frame[11], 8U);
}

WmBusMode5DecryptInfo wmbus_parser_decrypt_mode5(
    const uint8_t* frame,
    const size_t frame_len,
    uint16_t cfg,
    const uint8_t key[WMBUS_AES_BLOCK_LEN],
    uint8_t* out_frame) {
    WmBusMode5DecryptInfo info = {
        .result = WmBusDecryptResultInvalidArgs,
        .has_check_bytes = false,
    };

    if(!frame || !key || !out_frame) {
        return info;
    }
    if(frame_len > WMBUS_PARSER_FRAME_MAX || frame_len <= WMBUS_SHORT_TPL_POS)
        return (WmBusMode5DecryptInfo){
            .result = WmBusDecryptResultFrameTooShort,
            .has_check_bytes = false,
        };

    size_t encrypted_len = ((size_t)cfg >> 4) & 0x0FU;
    size_t payload_len = frame_len - WMBUS_SHORT_TPL_POS;
    uint8_t iv[WMBUS_AES_BLOCK_LEN] = {0};

    encrypted_len = encrypted_len ? (encrypted_len * WMBUS_AES_BLOCK_LEN) : payload_len;
    if(encrypted_len > payload_len) {
        encrypted_len = payload_len;
    }
    encrypted_len -= (encrypted_len % WMBUS_AES_BLOCK_LEN);
    if(encrypted_len < WMBUS_AES_BLOCK_LEN) {
        return (WmBusMode5DecryptInfo){
            .result = WmBusDecryptResultEncryptedPayloadTooShort,
            .has_check_bytes = false,
        };
    }

    memcpy(out_frame, frame, frame_len);
    wmbus_parser_build_mode5_iv(frame, iv);
    wmbus_aes128_cbc_decrypt_buffer(
        &out_frame[WMBUS_SHORT_TPL_POS],
        &frame[WMBUS_SHORT_TPL_POS],
        (uint32_t)encrypted_len,
        key,
        iv);

    info.result = WmBusDecryptResultOk;
    info.has_check_bytes = wmbus_parser_short_tpl_payload_has_check_bytes(out_frame, frame_len);
    return info;
}
