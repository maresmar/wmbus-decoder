#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "wmbus_parser.h"

typedef enum {
    WmBusRxModeT = 0,
    WmBusRxModeC = 1,
} WmBusRxMode;

typedef struct {
    uint8_t data[256];
    size_t len;
    size_t raw_len;
    int rssi;
    WmBusRxMode mode;
} WmBusCaptureFrame;

typedef struct {
    uint8_t raw[256];
    size_t raw_len;
    bool in_packet;
    size_t expected_raw_len;
    uint32_t last_byte_tick;
} WmBusCaptureStateT;

typedef struct {
    bool in_packet_signal;
    uint32_t dropped_invalid;
    uint32_t dropped_oversize;
} WmBusCaptureStateC;

void wmbus_capture_state_t_reset(WmBusCaptureStateT* state);
void wmbus_capture_state_c_reset(WmBusCaptureStateC* state);

bool wmbus_capture_l_field_valid(uint8_t l_field);

size_t wmbus_capture_frame_len_format_a(uint8_t l_field);
size_t wmbus_capture_frame_len_format_b(uint8_t l_field);

bool wmbus_capture_estimate_t_expected_raw_len(
    const uint8_t* raw,
    size_t raw_len,
    size_t raw_max,
    size_t* expected_raw_len);

bool wmbus_capture_reconstruct_c_frame(
    const uint8_t* payload,
    size_t payload_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len);
