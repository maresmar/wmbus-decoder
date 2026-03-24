#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../model/wmbus_application_types.h"
#include "wmbus_parser_context.h"
#include "wmbus_parser_packet.h"

bool wmbus_parser_dif_vif_probe(
    const WmBusParserPacketView* packet,
    const WmBusPacketParseContext* parse_context);
bool wmbus_parser_dif_vif_parse(
    const WmBusParserPacketView* packet,
    const WmBusPacketParseContext* parse_context,
    WmBusPacketApplicationData* out_application);
bool wmbus_packet_decode_application_records(
    const uint8_t* payload,
    size_t payload_len,
    WmBusApplicationRecord* out,
    uint8_t out_max,
    uint8_t* out_count);
uint8_t wmbus_packet_count_meaningful_records(
    const WmBusApplicationRecord* records,
    uint8_t record_count);
