#include "wmbus_parser_apator162.h"

#include "../wmbus_application_record.h"

#include <stdio.h>
#include <string.h>

#define TAG                        "apator162"
#define WMBUS_APATOR162_MFG_OLD    0x8614U
#define WMBUS_APATOR162_META_LEN   8U
#define WMBUS_APATOR162_STATUS_LEN 7U

typedef struct {
    size_t fields_pos;
    size_t status_pos;
    size_t status_len;
    bool has_status;
} WmBusApator162Layout;

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

static uint32_t wmbus_parser_apator162_read_u32_le(const uint8_t* data) {
    return (uint32_t)data[0] | ((uint32_t)data[1] << 8) | ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void
    wmbus_parser_apator162_store_total_record(WmBusPacketRecord* record, uint32_t total_m3_x1000) {
    WmBusApplicationRecord* app_record = NULL;
    if(!record || !wmbus_application_record_append(&record->application, &app_record)) return;

    app_record->quantity = WmBusApplicationQuantityVolume;
    app_record->scale10 = -3;
    app_record->data_len = 4U;
    wmbus_application_record_set_unsigned(app_record, total_m3_x1000);
}

static void wmbus_parser_apator162_store_status_record(
    WmBusPacketRecord* record,
    const uint8_t* status,
    size_t status_len) {
    WmBusApplicationRecord* app_record = NULL;
    if(!record || !status || status_len == 0U ||
       !wmbus_application_record_append(&record->application, &app_record)) {
        return;
    }

    app_record->quantity = WmBusApplicationQuantityStatus;
    app_record->data_len = (uint8_t)((status_len > UINT8_MAX) ? UINT8_MAX : status_len);
    if(!wmbus_application_record_set_raw_hex_le(app_record, status, app_record->data_len)) {
        app_record->value_type = WmBusApplicationValueRaw;
    }
}

/**
 * Locates the first Apator register in an application payload.
 *
 * Leading 0x2F filler bytes are skipped.
 *
 * The vendored `wmbusmeters` XMQ grammar models Apator 162 payloads as:
 * `2F* <1-byte proprietary marker> <7-byte status block> <register stream> FF*`
 */
static bool wmbus_parser_apator162_locate_layout(
    const uint8_t* payload,
    size_t payload_len,
    WmBusApator162Layout* layout) {
    if(!payload || !layout || payload_len == 0U) {
        return false;
    }

    size_t pos = 0U;
    while(pos < payload_len && payload[pos] == 0x2FU) {
        pos++;
    }

    if(pos >= payload_len) {
        return false;
    }

    memset(layout, 0, sizeof(*layout));
    if((payload_len - pos) < WMBUS_APATOR162_META_LEN) {
        return false;
    }

    layout->fields_pos = pos + WMBUS_APATOR162_META_LEN;
    layout->status_pos = pos + 1U;
    layout->status_len = WMBUS_APATOR162_STATUS_LEN;
    layout->has_status = true;

    if(layout->fields_pos >= payload_len) {
        return false;
    }
    if(payload[layout->fields_pos] != 0xFFU &&
       wmbus_parser_apator162_register_size(payload[layout->fields_pos]) < 0) {
        return false;
    }

    return true;
}

static bool wmbus_parser_apator162_scan_stream(
    const uint8_t* payload,
    size_t payload_len,
    const WmBusApator162Layout* layout,
    uint32_t* total_m3_x1000,
    bool* found_total) {
    if(!payload || !layout || layout->fields_pos >= payload_len) {
        return false;
    }

    bool saw_register = false;
    bool have_total = false;
    uint32_t parsed_total = 0U;
    size_t pos = layout->fields_pos;

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
        if(!have_total && reg_size >= 4 && (reg == 0x10U || reg == 0xA1U)) {
            parsed_total = wmbus_parser_apator162_read_u32_le(&payload[pos]);
            have_total = true;
        }

        pos += (size_t)reg_size;
    }

    if(!saw_register) {
        return false;
    }

    if(found_total) {
        *found_total = have_total;
    }
    if(total_m3_x1000 && have_total) {
        *total_m3_x1000 = parsed_total;
    }

    return true;
}

/**
 * Extracts the first total-volume value from the Apator register stream.
 *
 * Register 0x10 or 0xA1 is interpreted as a little-endian 32-bit total in m3*1000.
 */
bool wmbus_parser_parse_apator162_payload_total(
    const uint8_t* payload,
    size_t payload_len,
    uint32_t* total_m3_x1000) {
    if(!payload || !total_m3_x1000 || payload_len == 0U) {
        return false;
    }

    WmBusApator162Layout layout = {0};
    if(!wmbus_parser_apator162_locate_layout(payload, payload_len, &layout)) {
        return false;
    }

    uint32_t parsed_total = 0U;
    bool found_total = false;
    if(!wmbus_parser_apator162_scan_stream(
           payload, payload_len, &layout, &parsed_total, &found_total)) {
        return false;
    }
    if(!found_total) return false;

    *total_m3_x1000 = parsed_total;
    return true;
}

/**
 * Checks whether the record is likely an Apator 162 payload that this parser can handle.
 *
 * Requires short-TPL CI=0x7A and the known Apator 162 identity fields:
 * version 0x05, device type 0x06 or 0x07, and manufacturer "APA" or legacy 0x8614.
 */
bool wmbus_parser_apator162_probe(const WmBusPacketRecord* record) {
    if(!record || !record->payload.has_application_payload ||
       record->payload.application_len == 0U) {
        return false;
    }
    if(!record->tpl.has_short_tpl || record->dll.ci_field != 0x7AU) {
        return false;
    }
    if(record->dll.version != 0x05U ||
       (record->dll.dev_type != 0x06U && record->dll.dev_type != 0x07U)) {
        return false;
    }
    if(strcmp(record->dll.mfg, "APA") != 0 &&
       record->dll.m_field != WMBUS_APATOR162_MFG_OLD) {
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
    WmBusApator162Layout layout = {0};
    if(!wmbus_parser_apator162_locate_layout(
           record->payload.application_payload, record->payload.application_len, &layout)) {
        return false;
    }

    uint32_t total_m3_x1000 = 0U;
    bool found_total = false;
    if(!wmbus_parser_apator162_scan_stream(
           record->payload.application_payload,
           record->payload.application_len,
           &layout,
           &total_m3_x1000,
           &found_total) ||
       !found_total) {
        return false;
    }

    if(layout.has_status) {
        wmbus_parser_apator162_store_status_record(
            record, &record->payload.application_payload[layout.status_pos], layout.status_len);
    }
    wmbus_parser_apator162_store_total_record(record, total_m3_x1000);

    return true;
}
