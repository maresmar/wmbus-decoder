#include "wmbus_csv_sink.h"

#include "../../storage/wmbus_log.h"

static void wmbus_csv_sink_consume(
    void* context,
    const WmBusSettings* settings,
    const WmBusPacketRecord* record) {
    WmBusCsvSink* csv_sink = context;

    if(!csv_sink || !csv_sink->storage || !settings || !record) {
        return;
    }

    if(settings->csv_logging == WmBusCsvLoggingNone ||
       !wmbus_packet_record_passes_policy(record, settings->csv_quality, settings->min_rssi_dbm)) {
        return;
    }

    wmbus_log_append(csv_sink->storage, settings->csv_logging, record);
}

void wmbus_csv_sink_init(WmBusCsvSink* csv_sink, Storage* storage) {
    if(!csv_sink) {
        return;
    }

    *csv_sink = (WmBusCsvSink){
        .storage = storage,
        .sink =
            {
                .context = csv_sink,
                .consume = wmbus_csv_sink_consume,
            },
    };
}
