#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../wmbus_packet.h"

typedef struct {
    const char* name;
    bool (*probe)(const uint8_t* frame, size_t frame_len, const WmBusPacketRecord* record);
    bool (*parse)(const uint8_t* frame, size_t frame_len, WmBusPacketRecord* record);
} WmBusDeviceParser;

bool wmbus_device_parser_apply(
    const uint8_t* frame,
    size_t frame_len,
    WmBusPacketRecord* record);
