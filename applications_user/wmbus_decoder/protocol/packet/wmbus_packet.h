#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../capture/wmbus_capture.h"
#include "../crypto/wmbus_crypto_key_store.h"
#include "../model/wmbus_application_types.h"
#include "../../core/wmbus_types.h"
#include "wmbus_packet_parts.h"

#define WMBUS_PACKET_LABEL_MAX       32U
#define WMBUS_PACKET_VALUE_MAX       32U

typedef struct {
    WmBusStatus status;
    WmBusRxMode mode;
    bool decoded_ok;
    bool plausible;
    bool length_ok;
    bool crc_known;
    bool crc_ok;
    bool normalize_format_known;
    bool strong_rssi;
    uint16_t raw_len;
    uint16_t capture_len;
    uint16_t
        packet_len; /**< Stored byte count of `packet_bytes`, not the same as `dll.l_field`. */
    bool packet_is_frame;
    int best_offset;
    int rssi;
    uint32_t rx_tick;
    WmBusFrameFormat normalize_format;
    uint8_t capture_bytes[256];
    uint8_t packet_bytes[256];
    WmBusPacketDllData dll;
    WmBusPacketTplData tpl;
    WmBusPacketPayloadData payload;
    WmBusPacketIdentityData identity;
    WmBusPacketApplicationData application;
} WmBusPacketRecord;

bool wmbus_packet_process_capture(
    const WmBusCaptureFrame* capture,
    const WmBusCryptoKeyStore* key_store,
    WmBusPacketRecord* record);

const char* wmbus_packet_status_str(WmBusStatus status);
const char* wmbus_packet_status_short_label(WmBusStatus status);
const char* wmbus_packet_csv_logging_str(WmBusCsvLogging logging);
