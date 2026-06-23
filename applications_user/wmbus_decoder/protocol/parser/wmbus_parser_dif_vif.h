#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../model/wmbus_application_types.h"
#include "wmbus_parser_packet.h"

bool wmbus_parser_dif_vif_probe(const WmBusParserPacketView* packet);
bool wmbus_parser_dif_vif_parse(
    const WmBusParserPacketView* packet,
    WmBusPacketApplicationData* out_application);
bool wmbus_packet_decode_application_records(
    const uint8_t* payload,
    size_t payload_len,
    WmBusApplicationRecord* out,
    uint8_t out_max,
    uint8_t* out_count);
