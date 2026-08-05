#include "wmbus_capture.h"

#include "../decode/wmbus_decode.h"
#include "../frame/wmbus_frame.h"

#include <string.h>

#define WMBUS_C_SYNC_REMAINDER_0        0x54U
#define WMBUS_C_FRAME_A_SYNC_REMAINDER_1 0xCDU
#define WMBUS_C_FRAME_B_SYNC_REMAINDER_1 0x3DU
#define WMBUS_C_SYNC_REMAINDER_LEN 2U

void wmbus_fifo_capture_state_reset(WmBusFifoCaptureState* state) {
    if(!state) return;
    state->raw_len = 0;
    state->in_packet = false;
    state->last_activity_tick = 0;
}

void wmbus_fifo_capture_note_activity(WmBusFifoCaptureState* state, uint32_t tick) {
    if(!state) return;
    state->in_packet = true;
    state->last_activity_tick = tick;
}

WmBusCaptureLengthStatus wmbus_fifo_frame_length(
    WmBusRxMode mode,
    const uint8_t* fifo,
    size_t fifo_len,
    size_t* out_fifo_len,
    WmBusFrameFormat* out_format) {
    if(!fifo || !out_fifo_len || !out_format) return WmBusCaptureLengthInvalid;

    uint8_t l_field = 0U;
    WmBusFrameFormat format = WmBusFrameFormatUnknown;
    size_t expected_fifo_len = 0U;

    if(mode == WmBusRxModeC) {
        if(fifo_len < WMBUS_C_SYNC_REMAINDER_LEN) return WmBusCaptureLengthNeedMore;
        if(fifo[0] != WMBUS_C_SYNC_REMAINDER_0 ||
           (fifo[1] != WMBUS_C_FRAME_A_SYNC_REMAINDER_1 &&
            fifo[1] != WMBUS_C_FRAME_B_SYNC_REMAINDER_1)) {
            return WmBusCaptureLengthInvalid;
        }
        if(fifo_len < WMBUS_C_SYNC_REMAINDER_LEN + 1U) return WmBusCaptureLengthNeedMore;

        format = fifo[1] == WMBUS_C_FRAME_A_SYNC_REMAINDER_1 ? WmBusFrameFormatA :
                                                               WmBusFrameFormatB;
        l_field = fifo[WMBUS_C_SYNC_REMAINDER_LEN];
        if(!wmbus_frame_l_field_valid(l_field)) return WmBusCaptureLengthInvalid;
        expected_fifo_len = WMBUS_C_SYNC_REMAINDER_LEN +
                            wmbus_frame_expected_len(l_field, format);
    } else {
        if(mode != WmBusRxModeT) return WmBusCaptureLengthInvalid;
        if(fifo_len < 2U) return WmBusCaptureLengthNeedMore;

        size_t decoded_len = 0U;
        if(!wmbus_decode_3of6(fifo, 12U, &l_field, 1U, &decoded_len) || decoded_len != 1U ||
           !wmbus_frame_l_field_valid(l_field)) {
            return WmBusCaptureLengthInvalid;
        }

        /* T mode uses Frame A. Each decoded byte occupies exactly 12 air bits. */
        const size_t wire_len = wmbus_frame_expected_len(l_field, WmBusFrameFormatA);
        expected_fifo_len = (wire_len * 12U + 7U) / 8U;
        format = WmBusFrameFormatA;
    }

    if(expected_fifo_len == 0U || expected_fifo_len > WMBUS_PHY_FRAME_MAX_BYTES) {
        return WmBusCaptureLengthInvalid;
    }

    *out_fifo_len = expected_fifo_len;
    *out_format = format;
    return WmBusCaptureLengthKnown;
}

size_t wmbus_fifo_safe_read_size(
    WmBusRxMode mode,
    WmBusCaptureLengthStatus length_status,
    size_t captured_len,
    size_t expected_fifo_len,
    size_t available_fifo_len) {
    if(available_fifo_len == 0U || length_status == WmBusCaptureLengthInvalid) return 0U;

    size_t remaining = 0U;
    if(length_status == WmBusCaptureLengthKnown) {
        if(captured_len >= expected_fifo_len) return 0U;
        remaining = expected_fifo_len - captured_len;

        /* The complete software-delimited frame is now in FIFO. It is safe to
         * consume its final byte without retaining the CC1101 guard byte. */
        if(available_fifo_len >= remaining) return remaining;
    } else {
        const size_t header_len = mode == WmBusRxModeC ? 3U : 2U;
        if((mode != WmBusRxModeC && mode != WmBusRxModeT) || captured_len >= header_len) {
            return 0U;
        }
        remaining = header_len - captured_len;
    }

    /* CC1101 errata SWRZ020: never empty RX FIFO during active reception. */
    const size_t safe_available = available_fifo_len > 1U ? available_fifo_len - 1U : 0U;
    return remaining < safe_available ? remaining : safe_available;
}

bool wmbus_phy_frame_from_fifo(
    WmBusRxMode mode,
    const uint8_t* fifo,
    size_t fifo_len,
    int rssi,
    WmBusPhyFrame* frame) {
    if(!fifo || !frame || fifo_len == 0U) return false;
    memset(frame, 0, sizeof(*frame));

    size_t expected_fifo_len = 0U;
    WmBusFrameFormat format = WmBusFrameFormatUnknown;
    if(wmbus_fifo_frame_length(mode, fifo, fifo_len, &expected_fifo_len, &format) !=
           WmBusCaptureLengthKnown ||
       fifo_len < expected_fifo_len) {
        return false;
    }

    if(mode == WmBusRxModeC) {
        const size_t frame_len = expected_fifo_len - WMBUS_C_SYNC_REMAINDER_LEN;
        if(frame_len > sizeof(frame->data)) return false;
        memcpy(frame->data, &fifo[WMBUS_C_SYNC_REMAINDER_LEN], frame_len);
        frame->len = frame_len;
    } else if(mode == WmBusRxModeT) {
        uint8_t l_field = 0U;
        size_t l_field_len = 0U;
        if(!wmbus_decode_3of6(fifo, 12U, &l_field, 1U, &l_field_len) || l_field_len != 1U) {
            return false;
        }

        const size_t wire_len = wmbus_frame_expected_len(l_field, WmBusFrameFormatA);
        size_t decoded_len = 0U;
        if(!wmbus_decode_3of6(
               fifo,
               wire_len * 12U,
               frame->data,
               sizeof(frame->data),
               &decoded_len) ||
           decoded_len != wire_len) {
            return false;
        }
        frame->len = decoded_len;
    } else {
        return false;
    }
    frame->rssi = rssi;
    frame->mode = mode;
    frame->format = format;
    return true;
}
