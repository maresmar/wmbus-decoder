#pragma once

#include "../../protocol/packet/wmbus_packet.h"
#include "../../storage/wmbus_settings.h"

typedef struct {
    void* context;
    void (*consume)(
        void* context,
        const WmBusSettings* settings,
        const WmBusPacketRecord* record);
} WmBusPacketSink;

static inline void wmbus_packet_sink_consume(
    const WmBusPacketSink* sink,
    const WmBusSettings* settings,
    const WmBusPacketRecord* record) {
    if(!sink || !sink->consume) {
        return;
    }

    sink->consume(sink->context, settings, record);
}

