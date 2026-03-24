#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../capture/wmbus_capture.h"
#include "../model/wmbus_application_types.h"
#include "../parser/wmbus_parser_context.h"
#include "../../core/wmbus_types.h"
#include "../../storage/wmbus_keyring.h"
#include "wmbus_packet_parts.h"

#define WMBUS_PACKET_LABEL_MAX       32U
#define WMBUS_PACKET_VALUE_MAX       32U

typedef struct {
    char manufacturer[WMBUS_MFG_STR_LEN];
    char meter_id[WMBUS_ID_STR_LEN];
    bool meter_id_is_bcd;
} WmBusPacketIdentityData;

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
    uint16_t
        packet_len; /**< Stored byte count of `packet_bytes`, not the same as `dll.l_field`. */
    bool packet_is_frame;
    int best_offset;
    int rssi;
    uint32_t rx_tick;
    uint8_t packet_bytes[256];
    WmBusPacketDllData dll;
    WmBusPacketTplData tpl;
    WmBusPacketPayloadData payload;
    WmBusPacketIdentityData identity;
    WmBusPacketApplicationData application;
} WmBusPacketRecord;

bool wmbus_packet_process_capture(
    const WmBusCaptureFrame* capture,
    const WmBusKeyring* keyring,
    WmBusPacketRecord* record);

const char* wmbus_packet_status_str(WmBusStatus status);
const char* wmbus_packet_status_short_label(WmBusStatus status);
const char* wmbus_packet_csv_logging_str(WmBusCsvLogging logging);
