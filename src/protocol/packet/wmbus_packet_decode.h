#pragma once

#include "wmbus_packet.h"

typedef struct {
    const uint8_t* frame;
    size_t frame_len;
    WmBusPacketQuality quality;
} WmBusPacketDecodeState;

bool wmbus_packet_decode_capture(
    const WmBusCaptureFrame* capture,
    WmBusPacketRecord* record,
    uint8_t* frame_buf,
    size_t frame_buf_max,
    WmBusPacketDecodeState* out);

void wmbus_packet_store_frame(WmBusPacketRecord* record, const uint8_t* frame, size_t frame_len);
