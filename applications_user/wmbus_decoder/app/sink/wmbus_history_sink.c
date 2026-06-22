#include "wmbus_history_sink.h"

static void wmbus_history_sink_consume(
    void* context,
    const WmBusSettings* settings,
    const WmBusPacketRecord* record) {
    WmBusHistorySink* history_sink = context;

    if(!history_sink || !history_sink->rx_view || !settings || !record) {
        return;
    }

    bool store_in_history = wmbus_packet_record_passes_policy(
        record, settings->memory_quality, settings->min_rssi_dbm);
    wmbus_rx_view_push_packet(history_sink->rx_view, record, store_in_history);
}

void wmbus_history_sink_init(WmBusHistorySink* history_sink, WmBusRxView* rx_view) {
    if(!history_sink) {
        return;
    }

    *history_sink = (WmBusHistorySink){
        .rx_view = rx_view,
        .sink =
            {
                .context = history_sink,
                .consume = wmbus_history_sink_consume,
            },
    };
}

const WmBusPacketSink* wmbus_history_sink_get_packet_sink(const WmBusHistorySink* history_sink) {
    return history_sink ? &history_sink->sink : NULL;
}
