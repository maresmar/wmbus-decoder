#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "../model/wmbus_application_types.h"
#include "wmbus_parser_packet.h"

int wmbus_parser_apator162_register_size(uint8_t reg);
bool wmbus_parser_apator162_probe(const WmBusParserPacketView* packet);
bool wmbus_parser_apator162_parse(
    const WmBusParserPacketView* packet,
    WmBusPacketApplicationData* out_application);
