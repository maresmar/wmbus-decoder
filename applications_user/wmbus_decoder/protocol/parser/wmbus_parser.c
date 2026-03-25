#include "wmbus_parser.h"
#include "wmbus_device_parser.h"

#include "../../core/wmbus_types.h"

#define WMBUS_SHORT_TPL_POS    15U

static const WmBusParserInfo wmbus_builtin_parsers[] = {
    {
        .parser_id = WmBusParserIdUnknown,
        .name = "Unknown",
        .validates_decrypt = false,
        .show_detail = false,
    },
    {
        .parser_id = WmBusParserIdRaw,
        .name = "Raw",
        .validates_decrypt = false,
        .show_detail = false,
    },
    {
        .parser_id = WmBusParserIdHeader,
        .name = "Header",
        .validates_decrypt = false,
        .show_detail = false,
    },
    {
        .parser_id = WmBusParserIdShortTpl,
        .name = "Short TPL",
        .validates_decrypt = false,
        .show_detail = false,
    },
};

const WmBusParserInfo* wmbus_parser_get_info(WmBusParserId parser_id) {
    const WmBusDeviceParser* device_parser = wmbus_device_parser_get(parser_id);
    if(device_parser) {
        return &device_parser->info;
    }

    for(size_t i = 0; i < (sizeof(wmbus_builtin_parsers) / sizeof(wmbus_builtin_parsers[0]));
        i++) {
        if(wmbus_builtin_parsers[i].parser_id == parser_id) {
            return &wmbus_builtin_parsers[i];
        }
    }

    return &wmbus_builtin_parsers[0];
}

const char* wmbus_parser_id_name(WmBusParserId parser_id) {
    return wmbus_parser_get_info(parser_id)->name;
}

bool wmbus_parser_validates_decrypt(WmBusParserId parser_id) {
    return wmbus_parser_get_info(parser_id)->validates_decrypt;
}

bool wmbus_parser_show_detail(WmBusParserId parser_id) {
    return wmbus_parser_get_info(parser_id)->show_detail;
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
