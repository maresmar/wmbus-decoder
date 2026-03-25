#include "wmbus_capture_processor.h"

#include "sink/wmbus_csv_sink.h"
#include "sink/wmbus_history_sink.h"

#include "../protocol/packet/wmbus_packet.h"

#include <stdlib.h>
#include <string.h>

struct WmBusCaptureProcessor {
    WmBusCsvSink csv_sink;
    WmBusHistorySink history_sink;
    const WmBusPacketSink* sinks[2];
    size_t sink_count;
};

WmBusCaptureProcessor* wmbus_capture_processor_alloc(Storage* storage, WmBusRxView* rx_view) {
    if(!storage || !rx_view) {
        return NULL;
    }

    WmBusCaptureProcessor* processor = malloc(sizeof(*processor));
    if(!processor) {
        return NULL;
    }

    memset(processor, 0, sizeof(*processor));
    wmbus_csv_sink_init(&processor->csv_sink, storage);
    wmbus_history_sink_init(&processor->history_sink, rx_view);

    processor->sinks[processor->sink_count++] =
        wmbus_csv_sink_get_packet_sink(&processor->csv_sink);
    processor->sinks[processor->sink_count++] =
        wmbus_history_sink_get_packet_sink(&processor->history_sink);
    return processor;
}

void wmbus_capture_processor_free(WmBusCaptureProcessor* processor) {
    free(processor);
}

void wmbus_capture_processor_handle(
    WmBusCaptureProcessor* processor,
    const WmBusSettings* settings,
    const WmBusCryptoKeyStore* key_store,
    const WmBusCaptureFrame* capture) {
    if(!processor || !settings || !capture) {
        return;
    }

    WmBusPacketRecord record = {0};
    if(!wmbus_packet_process_capture(capture, key_store, &record)) {
        return;
    }

    for(size_t i = 0; i < processor->sink_count; i++) {
        wmbus_packet_sink_consume(processor->sinks[i], settings, &record);
    }
}
