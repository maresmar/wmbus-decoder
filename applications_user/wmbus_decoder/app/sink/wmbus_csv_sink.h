#pragma once

#include "wmbus_packet_sink.h"

#include <storage/storage.h>

typedef struct {
    Storage* storage;
    WmBusPacketSink sink;
} WmBusCsvSink;

void wmbus_csv_sink_init(WmBusCsvSink* csv_sink, Storage* storage);
const WmBusPacketSink* wmbus_csv_sink_get_packet_sink(const WmBusCsvSink* csv_sink);

