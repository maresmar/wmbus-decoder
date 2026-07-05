#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../core/wmbus_types.h"

typedef struct {
    uint8_t data[256];
    size_t len;
    int rssi;
    WmBusRxMode mode;
} WmBusCaptureFrame;

typedef struct {
    uint8_t raw[256];
    size_t raw_len;
    bool in_packet;
    uint32_t last_byte_tick;
} WmBusCaptureState;

void wmbus_capture_state_reset(WmBusCaptureState* state);
