#include "wmbus_capture.h"

void wmbus_capture_state_reset(WmBusCaptureState* state) {
    if(!state) return;
    state->raw_len = 0;
    state->in_packet = false;
    state->last_byte_tick = 0;
}
