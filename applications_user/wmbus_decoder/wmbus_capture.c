#include "wmbus_capture.h"

#include <string.h>

#define WMBUS_C_SIGNAL_BYTE 0x54U

void wmbus_capture_state_t_reset(WmBusCaptureStateT* state) {
    if(!state) return;
    state->raw_len = 0;
    state->in_packet = false;
    state->expected_raw_len = 0;
    state->last_byte_tick = 0;
}

void wmbus_capture_state_c_reset(WmBusCaptureStateC* state) {
    if(!state) return;
    state->raw_len = 0;
    state->in_packet = false;
    state->expected_len = 0;
    state->last_byte_tick = 0;
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

static size_t wmbus_capture_c_frame_offset(const uint8_t* raw, size_t raw_len) {
    if(!raw || raw_len == 0U) return SIZE_MAX;

    // TI's C-mode framing can prepend a signaling 0x54 byte after sync. When present,
    // the actual WM-Bus L-field is the next byte.
    if(raw_len >= 2U && raw[0] == WMBUS_C_SIGNAL_BYTE && wmbus_capture_l_field_valid(raw[1])) {
        return 1U;
    }

    if(wmbus_capture_l_field_valid(raw[0])) {
        return 0U;
    }

    return SIZE_MAX;
}

bool wmbus_capture_estimate_t_expected_raw_len(
    const uint8_t* raw,
    size_t raw_len,
    size_t raw_max,
    size_t* expected_raw_len) {
    if(!raw || !expected_raw_len) return false;

    uint8_t decoded[256] = {0};
    int best_score = -1;
    size_t best_expected = 0;

    for(uint8_t bit_offset = 0; bit_offset < 8U; bit_offset++) {
        for(uint8_t tail_pad = 0; tail_pad < 8U; tail_pad++) {
            size_t raw_bit_len = raw_len * 8U;
            if(raw_bit_len <= tail_pad) continue;
            raw_bit_len -= tail_pad;

            size_t decoded_len = 0;
            if(!wmbus_parser_decode_3of6_bits(
                   raw, raw_bit_len, bit_offset, decoded, sizeof(decoded), &decoded_len) ||
               decoded_len < 1) {
                continue;
            }

            uint8_t l_field = decoded[0];
            if(!wmbus_capture_l_field_valid(l_field)) continue;

            size_t expected_decoded_len = wmbus_capture_frame_len_format_a(l_field);
            size_t expected = (bit_offset + expected_decoded_len * 12U + 7U) / 8U;
            if(expected > raw_max) expected = raw_max;

            int score = 1;
            if(decoded_len >= 11U && wmbus_parser_is_plausible(decoded, decoded_len)) {
                score += 2;
            }

            if(score > best_score) {
                best_score = score;
                best_expected = expected;
            }
        }
    }

    if(best_score < 0) return false;

    *expected_raw_len = best_expected;
    return true;
}

bool wmbus_capture_estimate_c_expected_len(
    const uint8_t* raw,
    size_t raw_len,
    size_t raw_max,
    size_t* expected_len) {
    if(!raw || !expected_len || raw_len < 1) return false;

    size_t frame_offset = wmbus_capture_c_frame_offset(raw, raw_len);
    if(frame_offset == SIZE_MAX) return false;

    uint8_t l_field = raw[frame_offset];
    size_t expected = frame_offset + wmbus_capture_frame_len_format_a(l_field);
    if(expected > raw_max) expected = raw_max;
    *expected_len = expected;
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
