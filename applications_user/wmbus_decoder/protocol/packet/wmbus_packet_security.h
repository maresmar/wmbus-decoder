#pragma once

#include "wmbus_packet.h"
#include "../parser/wmbus_parser_context.h"

bool wmbus_packet_select_application(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record,
    WmBusPacketParseContext* parse_context,
    const WmBusCryptoKeyStore* key_store);

void wmbus_packet_finalize_parser(WmBusPacketRecord* record);
