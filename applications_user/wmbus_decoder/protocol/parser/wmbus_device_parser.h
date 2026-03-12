#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../wmbus_packet.h"

typedef struct {
    const char* name;
    bool (*probe)(const WmBusPacketRecord* record);
    bool (*parse)(WmBusPacketRecord* record);
} WmBusDeviceParser;

bool wmbus_device_parser_apply(WmBusPacketRecord* record);
