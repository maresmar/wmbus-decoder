#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../core/wmbus_types.h"

typedef struct {
    /*
     * Mode-specific bytes delivered to packet decoding:
     * T: raw 3-of-6 FIFO data; C: Link Layer wire frame starting at L-field.
     */
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
bool wmbus_capture_frame_from_fifo(
    WmBusRxMode mode,
    const uint8_t* fifo,
    size_t fifo_len,
    int rssi,
    WmBusCaptureFrame* frame);
