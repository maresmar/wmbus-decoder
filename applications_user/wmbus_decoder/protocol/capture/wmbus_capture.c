#include "wmbus_capture.h"

#include <string.h>

#include "../decode/wmbus_decode.h"
#include "../frame/wmbus_frame.h"

#define WMBUS_T_SYNC_SEARCH_BITS 64U

void wmbus_capture_state_t_reset(WmBusCaptureStateT* state) {
    if(!state) return;
    state->raw_len = 0;
    state->in_packet = false;
    state->expected_raw_len = 0;
    state->expected_raw_score = 0;
    state->last_byte_tick = 0;
}

void wmbus_capture_state_c_reset(WmBusCaptureStateC* state) {
    if(!state) return;
    state->raw_len = 0;
    state->in_packet = false;
    state->expected_len = 0;
    state->last_byte_tick = 0;
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

size_t wmbus_capture_c_frame_offset(const uint8_t* raw, size_t raw_len) {
    if(!raw || raw_len == 0U) return SIZE_MAX;

    if((raw_len >= 2U) && wmbus_capture_l_field_valid(raw[0]) &&
       wmbus_decode_c_field_valid(raw[1])) {
        return 0U;
    }

    return SIZE_MAX;
}

bool wmbus_capture_estimate_t_expected_raw_len_scored(
    const uint8_t* raw,
    size_t raw_len,
    size_t raw_max,
    size_t* expected_raw_len,
    int* expected_score) {
    if(!raw || !expected_raw_len || !expected_score) return false;

    uint8_t decoded[256] = {0};
    int best_score = -1;
    size_t best_expected = 0;

    size_t raw_bit_len = raw_len * 8U;
    // The CC1101 FIFO can include pre-frame bits before the first T-mode 3-of-6
    // symbol. Do not discard bytes blindly; search for a symbol boundary that
    // decodes to a credible L-field/header and use that to estimate frame length.
    size_t scan_bits = raw_bit_len;
    if(scan_bits > WMBUS_T_SYNC_SEARCH_BITS) {
        scan_bits = WMBUS_T_SYNC_SEARCH_BITS;
    }

    for(size_t bit_offset = 0; bit_offset < scan_bits; bit_offset++) {
        size_t decoded_len = 0;
        size_t l_bit_len = bit_offset + 12U;
        if(raw_bit_len < l_bit_len ||
           !wmbus_decode_3of6_bits(raw, l_bit_len, bit_offset, decoded, 1U, &decoded_len) ||
           decoded_len != 1U) {
            continue;
        }

        uint8_t l_field = decoded[0];
        if(!wmbus_capture_l_field_valid(l_field)) continue;

        size_t expected_decoded_len = wmbus_capture_frame_len_format_a(l_field);
        size_t expected_bit_len = bit_offset + expected_decoded_len * 12U;
        size_t expected = (expected_bit_len + 7U) / 8U;
        if(expected > raw_max) expected = raw_max;

        int score = 1;
        size_t header_bit_len = bit_offset + 11U * 12U;
        if(raw_bit_len >= header_bit_len &&
           wmbus_decode_3of6_bits(raw, header_bit_len, bit_offset, decoded, sizeof(decoded), &decoded_len) &&
           decoded_len >= 11U && wmbus_decode_is_plausible_frame(decoded, decoded_len)) {
            score += 2;
        }

        if(raw_bit_len >= expected_bit_len &&
           wmbus_decode_3of6_bits(
               raw, expected_bit_len, bit_offset, decoded, sizeof(decoded), &decoded_len) &&
           decoded_len >= expected_decoded_len) {
            uint8_t normalized[256] = {0};
            WmBusFrameNormalizeResult normalize = {0};
            if(wmbus_frame_normalize(
                   WmBusRxModeT,
                   decoded,
                   expected_decoded_len,
                   normalized,
                   sizeof(normalized),
                   &normalize) &&
               normalize.crc_ok) {
                score += 3;
            }
        }

        if(score > best_score) {
            best_score = score;
            best_expected = expected;
        }
    }

    if(best_score < 0) return false;

    *expected_raw_len = best_expected;
    *expected_score = best_score;
    return true;
}

bool wmbus_capture_estimate_t_expected_raw_len(
    const uint8_t* raw,
    size_t raw_len,
    size_t raw_max,
    size_t* expected_raw_len) {
    int expected_score = 0;
    return wmbus_capture_estimate_t_expected_raw_len_scored(
        raw, raw_len, raw_max, expected_raw_len, &expected_score);
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
    size_t expected_a = frame_offset + wmbus_capture_frame_len_format_a(l_field);
    size_t expected_b = frame_offset + wmbus_capture_frame_len_format_b(l_field);

    if(expected_b <= raw_max && raw_len >= expected_b &&
       wmbus_frame_crc_check(WmBusFrameFormatB, &raw[frame_offset], expected_b - frame_offset)) {
        *expected_len = expected_b;
        return true;
    }

    if(expected_a <= raw_max && raw_len >= expected_a &&
       wmbus_frame_crc_check(WmBusFrameFormatA, &raw[frame_offset], expected_a - frame_offset)) {
        *expected_len = expected_a;
        return true;
    }

    // When the format is still unknown, prefer the longer format-A expectation so
    // we never truncate a valid long frame before CRC disambiguation is possible.
    size_t expected = expected_a;
    if(expected > raw_max) expected = raw_max;
    *expected_len = expected;
    return true;
}

bool wmbus_capture_select_c_frame(
    const uint8_t* raw,
    size_t raw_len,
    size_t expected_len,
    size_t* frame_offset,
    size_t* frame_len) {
    if(!raw || !frame_offset || !frame_len || raw_len == 0U) return false;

    size_t selected_len = raw_len;
    if(expected_len > 0U && selected_len > expected_len) {
        selected_len = expected_len;
    }

    size_t selected_offset = wmbus_capture_c_frame_offset(raw, selected_len);
    if(selected_offset == SIZE_MAX || selected_len <= selected_offset) {
        return false;
    }

    *frame_offset = selected_offset;
    *frame_len = selected_len;
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
