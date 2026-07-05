#pragma once

#include "wmbus_packet_sink.h"

#include "../../ui/views/wmbus_rx_view.h"

typedef struct {
    WmBusRxView* rx_view;
    WmBusPacketSink sink;
} WmBusHistorySink;

void wmbus_history_sink_init(WmBusHistorySink* history_sink, WmBusRxView* rx_view);
