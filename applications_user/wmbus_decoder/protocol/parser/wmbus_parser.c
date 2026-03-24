#include "wmbus_parser.h"
#include "../crypto/wmbus_aes.h"

#include "../../core/wmbus_types.h"
#include <string.h>

#define WMBUS_PARSER_FRAME_MAX 256U
#define WMBUS_AES_BLOCK_LEN    16U
#define WMBUS_SHORT_TPL_POS    15U

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
