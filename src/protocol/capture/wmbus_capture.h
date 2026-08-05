#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "../../core/wmbus_types.h"

typedef struct {
    /* Decoded Link Layer wire frame starting at its L-field for every mode. */
    uint8_t data[256];
    size_t len;
    int rssi;
    WmBusRxMode mode;
    WmBusFrameFormat format;
} WmBusPhyFrame;

typedef enum {
    WmBusCaptureLengthNeedMore = 0,
    WmBusCaptureLengthKnown,
    WmBusCaptureLengthInvalid,
} WmBusCaptureLengthStatus;

typedef struct {
    uint8_t raw[256];
    size_t raw_len;
    bool in_packet;
    /* Last observed sync/FIFO activity; owns the incomplete-frame timeout. */
    uint32_t last_activity_tick;
} WmBusFifoCaptureState;

void wmbus_fifo_capture_state_reset(WmBusFifoCaptureState* state);
void wmbus_fifo_capture_note_activity(WmBusFifoCaptureState* state, uint32_t tick);
WmBusCaptureLengthStatus wmbus_fifo_frame_length(
    WmBusRxMode mode,
    const uint8_t* fifo,
    size_t fifo_len,
    size_t* out_fifo_len,
    WmBusFrameFormat* out_format);
size_t wmbus_fifo_safe_read_size(
    WmBusRxMode mode,
    WmBusCaptureLengthStatus length_status,
    size_t captured_len,
    size_t expected_fifo_len,
    size_t available_fifo_len);
bool wmbus_phy_frame_from_fifo(
    WmBusRxMode mode,
    const uint8_t* fifo,
    size_t fifo_len,
    int rssi,
    WmBusPhyFrame* frame);
