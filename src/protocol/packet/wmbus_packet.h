#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../capture/wmbus_capture.h"
#include "../crypto/wmbus_crypto_key_store.h"
#include "../model/wmbus_application_types.h"
#include "../../core/wmbus_types.h"
#include "wmbus_packet_parts.h"

#define WMBUS_PACKET_LABEL_MAX 32U
#define WMBUS_PACKET_VALUE_MAX 32U

typedef struct {
    WmBusPacketQuality quality;
    WmBusRxMode mode;
    uint16_t capture_len;
    uint16_t packet_len; /**< Stored byte count of `packet_bytes`, not the same as `dll.l_field`. */
    int best_offset;
    int rssi;
    uint32_t rx_tick;
    uint8_t capture_bytes[256];
    uint8_t packet_bytes[256];
    WmBusPacketDllData dll;
    WmBusPacketEllData ell;
    WmBusPacketTplData tpl;
    WmBusPacketPayloadData payload;
    WmBusPacketIdentityData identity;
    WmBusPacketApplicationData application;
} WmBusPacketRecord;

bool wmbus_packet_process_capture(
    const WmBusCaptureFrame* capture,
    const WmBusCryptoKeyStore* key_store,
    WmBusPacketRecord* record);

const char* wmbus_packet_quality_str(WmBusPacketQuality quality);
const char* wmbus_packet_quality_short_str(WmBusPacketQuality quality);
bool wmbus_packet_record_passes_policy(
    const WmBusPacketRecord* record,
    WmBusPacketQuality min_quality,
    int32_t min_rssi_dbm);
