#pragma once

#include <stddef.h>
#include <stdint.h>

#include "../packet/wmbus_packet_parts.h"

void wmbus_packet_summary_format_total_m3(
    uint32_t total_m3_x1000,
    char* out,
    size_t out_size,
    bool with_unit);
void wmbus_packet_summary_format_crypto_tag(
    const WmBusPacketEllData* ell,
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size);
void wmbus_packet_summary_format_security_text(
    const WmBusPacketEllData* ell,
    const WmBusPacketTplData* tpl,
    char* out,
    size_t out_size);
