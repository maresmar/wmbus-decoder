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
