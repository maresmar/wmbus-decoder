#include "wmbus_capture_processor.h"

#include "../protocol/packet/wmbus_packet.h"

#include <stdlib.h>
#include <string.h>

#define WMBUS_CAPTURE_PROCESSOR_MAX_SINKS 4U

struct WmBusCaptureProcessor {
    const WmBusPacketSink* sinks[WMBUS_CAPTURE_PROCESSOR_MAX_SINKS];
    size_t sink_count;
};

WmBusCaptureProcessor* wmbus_capture_processor_alloc(void) {
    WmBusCaptureProcessor* processor = malloc(sizeof(*processor));
    if(!processor) {
        return NULL;
    }

    memset(processor, 0, sizeof(*processor));
    return processor;
}

void wmbus_capture_processor_free(WmBusCaptureProcessor* processor) {
    free(processor);
}

bool wmbus_capture_processor_add_sink(
    WmBusCaptureProcessor* processor,
    const WmBusPacketSink* sink) {
    if(!processor || !sink || !sink->consume ||
       processor->sink_count >= WMBUS_CAPTURE_PROCESSOR_MAX_SINKS) {
        return false;
    }

    processor->sinks[processor->sink_count++] = sink;
    return true;
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

    record.rssi_ok = (settings->min_rssi_dbm >= 0) || (record.rssi >= settings->min_rssi_dbm);
    if(!record.rssi_ok && record.status == WmBusStatusOk) {
        record.status = WmBusStatusWeakRssi;
    }

    for(size_t i = 0; i < processor->sink_count; i++) {
        processor->sinks[i]->consume(processor->sinks[i]->context, settings, &record);
    }
}
