#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../wmbus_packet.h"

typedef struct {
    WmBusParserId parser_id;
    bool (*probe)(const WmBusPacketRecord* record, const WmBusPacketParseContext* parse_context);
    bool (*parse)(WmBusPacketRecord* record, const WmBusPacketParseContext* parse_context);
} WmBusDeviceParser;

bool wmbus_device_parser_apply(
    WmBusPacketRecord* record,
    const WmBusPacketParseContext* parse_context);
