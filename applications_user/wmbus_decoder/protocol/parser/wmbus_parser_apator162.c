#include "wmbus_parser_apator162.h"

#include <stdio.h>
#include <string.h>

#define WMBUS_SHORT_TPL_POS 15U
#define WMBUS_APATOR162_MFG_OLD 0x8614U

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

static bool wmbus_parser_apator162_payload_start(
    const uint8_t* payload,
    size_t payload_len,
    size_t* start_pos) {
    if(!payload || !start_pos || payload_len == 0U) {
        return false;
    }

    size_t pos = 0U;
    while(pos < payload_len && payload[pos] == 0x2FU) {
        pos++;
    }

    if(pos >= payload_len) {
        return false;
    }

    if(payload[pos] == 0x0FU) {
        if((payload_len - pos) < 8U) {
            return false;
        }
        pos += 8U;
    } else if(wmbus_parser_apator162_register_size(payload[pos]) < 0 && payload[pos] != 0xFFU) {
        return false;
    }

    *start_pos = pos;
    return true;
}

bool wmbus_parser_validate_apator162_payload(const uint8_t* payload, size_t payload_len) {
    size_t pos = 0U;
    bool saw_register = false;

    if(!wmbus_parser_apator162_payload_start(payload, payload_len, &pos)) {
        return false;
    }

    while(pos < payload_len) {
        uint8_t reg = payload[pos++];
        if(reg == 0xFFU) {
            break;
        }

        int reg_size = wmbus_parser_apator162_register_size(reg);
        if(reg_size < 0 || pos + (size_t)reg_size > payload_len) {
            return false;
        }

        saw_register = true;
        pos += (size_t)reg_size;
    }

    return saw_register;
}

bool wmbus_parser_parse_apator162_payload_total(
    const uint8_t* payload,
    size_t payload_len,
    uint32_t* total_m3_x1000) {
    if(!payload || !total_m3_x1000 || payload_len == 0U) {
        return false;
    }

    size_t pos = 0U;
    if(!wmbus_parser_apator162_payload_start(payload, payload_len, &pos)) {
        return false;
    }

    bool have_total = false;
    uint32_t parsed_total = 0U;

    while(pos < payload_len) {
        uint8_t reg = payload[pos++];
        if(reg == 0xFFU) {
            break;
        }

        int reg_size = wmbus_parser_apator162_register_size(reg);
        if(reg_size < 0 || pos + (size_t)reg_size > payload_len) {
            if(!have_total) {
                return false;
            }
            *total_m3_x1000 = parsed_total;
            return true;
        }

        if(!have_total && (reg == 0x10U || reg == 0xA1U) && reg_size >= 4) {
            parsed_total = (uint32_t)payload[pos] | ((uint32_t)payload[pos + 1] << 8) |
                           ((uint32_t)payload[pos + 2] << 16) | ((uint32_t)payload[pos + 3] << 24);
            have_total = true;
        }

        pos += (size_t)reg_size;
    }

    if(!have_total) {
        return false;
    }

    *total_m3_x1000 = parsed_total;
    return true;
}

static bool wmbus_parser_apator162_identity_matches(const WmBusPacketRecord* record) {
    if(!record) {
        return false;
    }

    if(record->data.version != 0x05U || record->data.dev_type != 0x07U) {
        return false;
    }

    return strcmp(record->data.mfg, "APA") == 0 || record->data.m_field == WMBUS_APATOR162_MFG_OLD;
}

bool wmbus_parser_apator162_probe(
    const uint8_t* frame,
    size_t frame_len,
    const WmBusPacketRecord* record) {
    if(!frame || !record || frame_len <= WMBUS_SHORT_TPL_POS) {
        return false;
    }
    if(!record->data.has_short_tpl || record->data.ci_field != 0x7AU) {
        return false;
    }
    if(!wmbus_parser_apator162_identity_matches(record)) {
        return false;
    }

    const uint8_t* payload = &frame[WMBUS_SHORT_TPL_POS];
    size_t payload_len = frame_len - WMBUS_SHORT_TPL_POS;
    if(wmbus_parser_validate_apator162_payload(payload, payload_len)) {
        return true;
    }

    uint32_t total_m3_x1000 = 0U;
    return wmbus_parser_parse_apator162_payload_total(payload, payload_len, &total_m3_x1000);
}

bool wmbus_parser_apator162_parse(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record) {
    if(!wmbus_parser_apator162_probe(frame, frame_len, record)) {
        return false;
    }

    snprintf(record->data.parser_name, sizeof(record->data.parser_name), "Apator162");

    uint32_t total_m3_x1000 = 0U;
    if(wmbus_parser_parse_apator162_payload_total(
           &frame[WMBUS_SHORT_TPL_POS], frame_len - WMBUS_SHORT_TPL_POS, &total_m3_x1000)) {
        record->data.has_total_m3 = true;
        record->data.total_m3_x1000 = total_m3_x1000;
    }

    return true;
}
