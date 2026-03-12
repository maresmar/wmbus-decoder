#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "capture/wmbus_capture.h"
#include "../core/wmbus_types.h"
#include "../storage/wmbus_keyring.h"

#define WMBUS_PACKET_FIELD_MAX       12U
#define WMBUS_PACKET_LABEL_MAX       16U
#define WMBUS_PACKET_VALUE_MAX       32U
#define WMBUS_PACKET_PARSER_NAME_MAX 16U

typedef struct {
    char label[WMBUS_PACKET_LABEL_MAX];
    char value[WMBUS_PACKET_VALUE_MAX];
} WmBusPacketField;

typedef struct {
    uint8_t l_field;
    uint8_t c_field;
    uint16_t m_field;
    char mfg[4];
    uint8_t id[4];
    char id_str[9];
    bool id_is_bcd;
    uint8_t version;
    uint8_t dev_type;
    uint8_t ci_field;

    bool has_short_tpl;
    uint8_t acc;
    uint8_t tpl_status;
    uint16_t cfg;
    uint8_t security_mode;
    bool security_likely_encrypted;
    bool decrypted;
    bool key_applied;
    uint8_t key_index;

    char parser_name[WMBUS_PACKET_PARSER_NAME_MAX];
    char primary_a[WMBUS_PACKET_VALUE_MAX];
    char primary_b[WMBUS_PACKET_VALUE_MAX];
    bool has_total_m3;
    uint32_t total_m3_x1000;
    uint8_t field_count;
    WmBusPacketField fields[WMBUS_PACKET_FIELD_MAX];
} WmBusPacketData;

typedef struct {
    WmBusStatus status;
    WmBusRxMode mode;
    bool decoded_ok;
    bool plausible;
    bool length_ok;
    bool crc_known;
    bool crc_ok;
    bool strong_rssi;
    uint16_t raw_len;
    uint16_t packet_len;
    bool packet_is_frame;
    int best_offset;
    int rssi;
    uint32_t rx_tick;
    uint8_t packet_bytes[256];
    WmBusPacketData data;
} WmBusPacketRecord;

bool wmbus_packet_process_capture(
    const WmBusCaptureFrame* capture,
    const WmBusKeyring* keyring,
    WmBusPacketRecord* record);

const char* wmbus_packet_status_str(WmBusStatus status);
const char* wmbus_packet_status_short_label(WmBusStatus status);
const char* wmbus_packet_csv_logging_str(WmBusCsvLogging logging);
void wmbus_packet_format_total_m3(uint32_t total_m3_x1000, char* out, size_t out_size);
void wmbus_packet_format_security_summary(
    bool has_short_tpl,
    uint8_t security_mode,
    bool security_likely_encrypted,
    bool decrypted,
    bool key_applied,
    uint8_t key_index,
    char* out,
    size_t out_size);
void wmbus_packet_format_security_text(
    bool has_short_tpl,
    uint8_t security_mode,
    bool security_likely_encrypted,
    bool decrypted,
    bool key_applied,
    uint8_t key_index,
    char* out,
    size_t out_size);
void wmbus_packet_build_fields_text(const WmBusPacketRecord* record, char* out, size_t out_size);
void wmbus_packet_build_detail_text(const WmBusPacketRecord* record, char* out, size_t out_size);
