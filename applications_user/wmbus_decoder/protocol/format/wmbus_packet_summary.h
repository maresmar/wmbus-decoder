#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../packet/wmbus_packet.h"
#include "../packet/wmbus_packet_parts.h"

bool wmbus_packet_summary_find_total_m3(
    const WmBusPacketApplicationData* application,
    uint32_t* total_m3_x1000);
void wmbus_packet_summary_format_total_m3(
    uint32_t total_m3_x1000,
    char* out,
    size_t out_size,
    bool with_unit);
void wmbus_packet_summary_format_crypto_tag(
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size);
void wmbus_packet_summary_format_security_text(
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size);
void wmbus_packet_summary_format_bottom_line(
    WmBusRxMode mode,
    int rssi,
    bool packet_is_frame,
    uint16_t packet_len,
    const WmBusPacketDllData* dll,
    const WmBusPacketTplData* tpl,
    bool has_total_volume,
    uint32_t total_m3_x1000,
    char* out,
    size_t out_size);
