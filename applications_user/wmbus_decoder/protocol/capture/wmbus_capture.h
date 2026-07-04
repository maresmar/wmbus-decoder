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
} WmBusCaptureStateT;

typedef struct {
    uint8_t raw[256];
    size_t raw_len;
    bool in_packet;
    uint32_t last_byte_tick;
} WmBusCaptureStateC;

void wmbus_capture_state_t_reset(WmBusCaptureStateT* state);
void wmbus_capture_state_c_reset(WmBusCaptureStateC* state);

bool wmbus_capture_l_field_valid(uint8_t l_field);

size_t wmbus_capture_frame_len_format_a(uint8_t l_field);
size_t wmbus_capture_frame_len_format_b(uint8_t l_field);

bool wmbus_capture_reconstruct_c_frame(
    const uint8_t* payload,
    size_t payload_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len);
