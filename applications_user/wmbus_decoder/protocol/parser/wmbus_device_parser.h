#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../model/wmbus_application_types.h"
#include "wmbus_parser.h"
#include "wmbus_parser_packet.h"

typedef struct {
    WmBusParserInfo info;
    bool (*probe)(const WmBusParserPacketView* packet);
    bool (*parse)(const WmBusParserPacketView* packet, WmBusPacketApplicationData* out_application);
} WmBusDeviceParser;

bool wmbus_device_parser_apply(
    const WmBusParserPacketView* packet,
    WmBusPacketApplicationData* out_application);
bool wmbus_device_parser_validate_decrypt(
    const WmBusParserPacketView* packet,
    WmBusParserId* out_parser_id);
const WmBusDeviceParser* wmbus_device_parser_get(WmBusParserId parser_id);
