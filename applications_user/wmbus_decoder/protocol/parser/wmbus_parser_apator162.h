#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../wmbus_packet.h"

int wmbus_parser_apator162_register_size(uint8_t reg);
bool wmbus_parser_parse_apator162_payload_total(
    const uint8_t* payload,
    size_t payload_len,
    uint32_t* total_m3_x1000);
bool wmbus_parser_apator162_probe(
    const WmBusPacketRecord* record,
    const WmBusPacketParseContext* parse_context);
bool wmbus_parser_apator162_parse(
    WmBusPacketRecord* record,
    const WmBusPacketParseContext* parse_context);
