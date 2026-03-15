#include "wmbus_parser_apator162.h"

#include <stdio.h>
#include <string.h>

#define WMBUS_APATOR162_MFG_OLD 0x8614U
#define WMBUS_APATOR162_META_LEN 8U
#define WMBUS_APATOR162_STATUS_LEN 7U

/**
 * Returns the payload size for a single Apator register body.
 *
 * The size excludes the register byte itself. Unknown register codes return -1.
 */
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

/**
 * Checks whether a payload tail can be consumed as an Apator register stream.
 */
static bool wmbus_parser_apator162_stream_valid(
    const uint8_t* payload,
    size_t payload_len,
    size_t pos) {
    if(!payload || pos >= payload_len) {
        return false;
    }

    bool saw_register = false;
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

static bool wmbus_parser_apator162_scan_total_from_stream(
    const uint8_t* payload,
    size_t payload_len,
    size_t pos,
    uint32_t* total_m3_x1000) {
    if(!payload || !total_m3_x1000 || pos >= payload_len) {
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
            break;
        }

        if(!have_total && reg == 0x10U && reg_size >= 4) {
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

static bool wmbus_parser_apator162_append_record(
    WmBusPacketRecord* record,
    WmBusApplicationRecord** out_record) {
    if(!record || !out_record ||
       record->application.record_count >= COUNT_OF(record->application.records)) {
        return false;
    }

    WmBusApplicationRecord* app_record = &record->application.records[record->application.record_count];
    memset(app_record, 0, sizeof(*app_record));
    record->application.record_count++;
    *out_record = app_record;
    return true;
}

static void
    wmbus_parser_apator162_store_total_record(WmBusPacketRecord* record, uint32_t total_m3_x1000) {
    WmBusApplicationRecord* app_record = NULL;
    if(!wmbus_parser_apator162_append_record(record, &app_record)) return;

    app_record->quantity = WmBusApplicationQuantityVolume;
    app_record->value_type = WmBusApplicationValueUnsigned;
    app_record->value_unsigned = total_m3_x1000;
    app_record->scale10 = -3;
    app_record->data_len = 4U;
    app_record->raw_len = 4U;
    app_record->raw[0] = (uint8_t)(total_m3_x1000 & 0xFFU);
    app_record->raw[1] = (uint8_t)((total_m3_x1000 >> 8) & 0xFFU);
    app_record->raw[2] = (uint8_t)((total_m3_x1000 >> 16) & 0xFFU);
    app_record->raw[3] = (uint8_t)((total_m3_x1000 >> 24) & 0xFFU);
}

static void wmbus_parser_apator162_store_status_record(
    WmBusPacketRecord* record,
    const uint8_t* status,
    size_t status_len) {
    WmBusApplicationRecord* app_record = NULL;
    if(!status || status_len == 0U || !wmbus_parser_apator162_append_record(record, &app_record)) {
        return;
    }

    if(status_len > sizeof(app_record->raw)) {
        status_len = sizeof(app_record->raw);
    }

    app_record->quantity = WmBusApplicationQuantityStatus;
    app_record->value_type = WmBusApplicationValueRaw;
    app_record->data_len = (uint8_t)status_len;
    app_record->raw_len = (uint8_t)status_len;
    memcpy(app_record->raw, status, status_len);
}

/**
 * Locates the first Apator register in an application payload.
 *
 * Leading 0x2F filler bytes are skipped.
 *
 * Current upstream `wmbusmeters` models Apator 162 payloads as:
 * `2F* <1-byte proprietary marker> <7-byte status block> <register stream> FF*`
 *
 * The first 8 bytes are manufacturer-specific metadata and are not decoded
 * semantically here. The register stream starts immediately after them.
 */
static bool wmbus_parser_apator162_scan_meta(
    const uint8_t* payload,
    size_t payload_len,
    size_t* meta_pos,
    size_t* fields_pos) {
    if(!payload || !meta_pos || !fields_pos || payload_len == 0U) {
        return false;
    }

    size_t pos = 0U;
    while(pos < payload_len && payload[pos] == 0x2FU) {
        pos++;
    }

    if(pos >= payload_len) {
        return false;
    }

    if((payload_len - pos) < WMBUS_APATOR162_META_LEN) {
        return false;
    }

    *meta_pos = pos;
    *fields_pos = pos + WMBUS_APATOR162_META_LEN;
    return true;
}

static bool wmbus_parser_apator162_payload_start(
    const uint8_t* payload,
    size_t payload_len,
    size_t* start_pos) {
    size_t meta_pos = 0U;
    size_t pos = 0U;
    if(!wmbus_parser_apator162_scan_meta(payload, payload_len, &meta_pos, &pos)) {
        return false;
    }

    if(!wmbus_parser_apator162_stream_valid(payload, payload_len, pos)) {
        return false;
    }

    *start_pos = pos;
    return true;
}


/**
 * Verifies that the payload can be walked as an Apator register stream.
 *
 * Returns true only if the payload start is valid and at least one complete register
 * can be consumed before the optional 0xFF terminator.
 */
bool wmbus_parser_validate_apator162_payload(const uint8_t* payload, size_t payload_len) {
    size_t pos = 0U;
    if(!wmbus_parser_apator162_payload_start(payload, payload_len, &pos)) {
        return false;
    }

    return wmbus_parser_apator162_stream_valid(payload, payload_len, pos);
}

/**
 * Extracts the first total-volume value from the Apator register stream.
 *
 * Register 0x10 is interpreted as a little-endian 32-bit total in m3*1000.
 * Returns true once such a value was found, even if a later register is malformed.
 */
bool wmbus_parser_parse_apator162_payload_total(
    const uint8_t* payload,
    size_t payload_len,
    uint32_t* total_m3_x1000) {
    if(!payload || !total_m3_x1000 || payload_len == 0U) {
        return false;
    }

    size_t meta_pos = 0U;
    size_t pos = 0U;
    if(!wmbus_parser_apator162_scan_meta(payload, payload_len, &meta_pos, &pos)) {
        return false;
    }
    return wmbus_parser_apator162_scan_total_from_stream(payload, payload_len, pos, total_m3_x1000);
}

/**
 * Matches the fixed meter identity fields used by known Apator 162 devices.
 *
 * Both the current "APA" manufacturer code and the legacy numeric code are accepted.
 */
static bool wmbus_parser_apator162_identity_matches(const WmBusPacketRecord* record) {
    if(!record) {
        return false;
    }

    if(record->frame.version != 0x05U || record->frame.dev_type != 0x07U) {
        return false;
    }

    return strcmp(record->frame.mfg, "APA") == 0 ||
           record->frame.m_field == WMBUS_APATOR162_MFG_OLD;
}

/**
 * Checks whether the record is likely an Apator 162 payload that this parser can handle.
 *
 * Probe succeeds either for a fully valid register stream or for a payload where only
 * the total-volume register can be recovered.
 */
bool wmbus_parser_apator162_probe(const WmBusPacketRecord* record) {
    if(!record || !record->payload.has_app_payload || record->payload.app_len == 0U) {
        return false;
    }
    if(!record->transport.has_short_tpl || record->frame.ci_field != 0x7AU) {
        return false;
    }
    if(!wmbus_parser_apator162_identity_matches(record)) {
        return false;
    }
    return true;
}

/**
 * Populates reusable application records from an Apator 162 payload.
 */
bool wmbus_parser_apator162_parse(WmBusPacketRecord* record) {
    if(!wmbus_parser_apator162_probe(record)) {
        return false;
    }

    record->application.record_count = 0U;
    size_t meta_pos = 0U;
    size_t fields_pos = 0U;
    if(!wmbus_parser_apator162_scan_meta(
           record->payload.app_payload, record->payload.app_len, &meta_pos, &fields_pos)) {
        return false;
    }

    uint32_t total_m3_x1000 = 0U;
    bool have_total = wmbus_parser_apator162_scan_total_from_stream(
        record->payload.app_payload, record->payload.app_len, fields_pos, &total_m3_x1000);
    bool payload_valid = wmbus_parser_apator162_stream_valid(
        record->payload.app_payload, record->payload.app_len, fields_pos);

    if(!have_total && !payload_valid) {
        return false;
    }
    if(!have_total) {
        return false;
    }

    wmbus_parser_apator162_store_status_record(
        record,
        &record->payload.app_payload[meta_pos + 1U],
        WMBUS_APATOR162_STATUS_LEN);
    wmbus_parser_apator162_store_total_record(record, total_m3_x1000);

    return true;
}
