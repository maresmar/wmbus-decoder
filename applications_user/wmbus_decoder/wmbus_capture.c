#include "wmbus_capture.h"

#include <string.h>

void wmbus_capture_state_t_reset(WmBusCaptureStateT* state) {
    if(!state) return;
    state->raw_len = 0;
    state->in_packet = false;
    state->expected_raw_len = 0;
    state->last_byte_tick = 0;
}

void wmbus_capture_state_c_reset(WmBusCaptureStateC* state) {
    if(!state) return;
    state->in_packet_signal = false;
    state->dropped_invalid = 0;
    state->dropped_oversize = 0;
}

bool wmbus_capture_l_field_valid(uint8_t l_field) {
    return l_field >= 10;
}

size_t wmbus_capture_frame_len_format_a(uint8_t l_field) {
    size_t n = l_field;
    size_t len = 1U + n + 2U; // L + data + first CRC
    if(n > 9U) {
        size_t rem = n - 9U;
        size_t blocks = (rem + 15U) / 16U;
        len += 2U * blocks;
    }
    return len;
}

size_t wmbus_capture_frame_len_format_b(uint8_t l_field) {
    return 1U + (size_t)l_field;
}

bool wmbus_capture_estimate_t_expected_raw_len(
    const uint8_t* raw,
    size_t raw_len,
    size_t raw_max,
    size_t* expected_raw_len) {
    if(!raw || !expected_raw_len) return false;

    size_t decoded_len = 0;
    uint8_t decoded[256] = {0};
    if(!wmbus_parser_decode_3of6(raw, raw_len, decoded, sizeof(decoded), &decoded_len) || decoded_len < 1) {
        return false;
    }

    uint8_t l_field = decoded[0];
    if(!wmbus_capture_l_field_valid(l_field)) return false;

    size_t expected_decoded_len = wmbus_capture_frame_len_format_a(l_field);
    size_t expected = (expected_decoded_len * 12U + 7U) / 8U;
    if(expected > raw_max) expected = raw_max;
    *expected_raw_len = expected;
    return true;
}

bool wmbus_capture_reconstruct_c_frame(
    const uint8_t* payload,
    size_t payload_len,
    uint8_t* out,
    size_t out_max,
    size_t* out_len) {
    if(!payload || !out || !out_len) return false;
    if(payload_len > 0xFFU) return false;
    if(payload_len + 1U > out_max) return false;

    out[0] = (uint8_t)payload_len;
    if(payload_len > 0) {
        memcpy(&out[1], payload, payload_len);
    }
    *out_len = payload_len + 1U;
    return true;
}
