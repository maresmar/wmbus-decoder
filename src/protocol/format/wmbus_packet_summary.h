#pragma once

#include <stddef.h>

#include "../packet/wmbus_packet_parts.h"

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
uint8_t wmbus_packet_summary_security_mode(
    const WmBusPacketEllData* ell,
    const WmBusPacketTplData* tpl);
uint8_t
    wmbus_packet_summary_key_index(const WmBusPacketEllData* ell, const WmBusPacketTplData* tpl);
