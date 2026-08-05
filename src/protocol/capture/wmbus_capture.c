#include "wmbus_capture.h"

#include <string.h>

#define WMBUS_C_SYNC_REMAINDER_0        0x54U
#define WMBUS_C_FRAME_A_SYNC_REMAINDER_1 0xCDU
#define WMBUS_C_FRAME_B_SYNC_REMAINDER_1 0x3DU
#define WMBUS_C_SYNC_REMAINDER_LEN 2U

void wmbus_capture_state_reset(WmBusCaptureState* state) {
    if(!state) return;
    state->raw_len = 0;
    state->in_packet = false;
    state->last_byte_tick = 0;
}

bool wmbus_capture_frame_from_fifo(
    WmBusRxMode mode,
    const uint8_t* fifo,
    size_t fifo_len,
    int rssi,
    WmBusCaptureFrame* frame) {
    if(!fifo || !frame || fifo_len == 0U) return false;
    memset(frame, 0, sizeof(*frame));

    size_t frame_offset = 0U;
    if(mode == WmBusRxModeC) {
        /*
         * The CC1101 has already matched the first 16 C-mode sync bits
         * (54 3D).  The next FIFO bytes must be the remaining sync bits:
         * 54 CD for Frame A or 54 3D for Frame B.  The Link Layer L-field
         * follows immediately after this format-specific sync remainder.
         */
        if(fifo_len <= WMBUS_C_SYNC_REMAINDER_LEN ||
           fifo[0] != WMBUS_C_SYNC_REMAINDER_0 ||
           (fifo[1] != WMBUS_C_FRAME_A_SYNC_REMAINDER_1 &&
            fifo[1] != WMBUS_C_FRAME_B_SYNC_REMAINDER_1)) {
            return false;
        }
        frame_offset = WMBUS_C_SYNC_REMAINDER_LEN;
    }

    size_t frame_len = fifo_len - frame_offset;
    if(frame_len > sizeof(frame->data)) return false;

    memcpy(frame->data, &fifo[frame_offset], frame_len);
    frame->len = frame_len;
    frame->rssi = rssi;
    frame->mode = mode;
    return true;
}
