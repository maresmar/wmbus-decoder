#pragma once

#include <stdbool.h>
#include <stdint.h>

#define WMBUS_PACKET_PAYLOAD_MAX 256U

typedef struct {
    bool has_application_payload;
    uint16_t application_len;
    const uint8_t* application_payload;
    uint8_t application_payload_storage[WMBUS_PACKET_PAYLOAD_MAX];
} WmBusPacketParseContext;
