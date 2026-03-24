#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../model/wmbus_application_types.h"
#include "wmbus_parser_context.h"
#include "wmbus_parser_packet.h"

typedef struct {
    WmBusParserId parser_id;
    bool (*probe)(
        const WmBusParserPacketView* packet,
        const WmBusPacketParseContext* parse_context);
    bool (*parse)(
        const WmBusParserPacketView* packet,
        const WmBusPacketParseContext* parse_context,
        WmBusPacketApplicationData* out_application);
} WmBusDeviceParser;

bool wmbus_device_parser_apply(
    const WmBusParserPacketView* packet,
    const WmBusPacketParseContext* parse_context,
    WmBusPacketApplicationData* out_application);
