#include "wmbus_capture_processor.h"

#include "../protocol/packet/wmbus_packet.h"
#include "../storage/wmbus_log.h"

void wmbus_capture_processor_handle(
    Storage* storage,
    WmBusRxView* rx_view,
    const WmBusSettings* settings,
    const WmBusKeyring* keyring,
    const WmBusCaptureFrame* capture) {
    if(!storage || !rx_view || !settings || !capture) {
        return;
    }

    WmBusPacketRecord record = {0};
    if(!wmbus_packet_process_capture(capture, keyring, &record)) {
        return;
    }

    if((settings->csv_logging != WmBusCsvLoggingNone) &&
       wmbus_status_meets_threshold(record.status, settings->csv_threshold)) {
        wmbus_log_append(storage, settings->csv_logging, &record);
    }

    bool store_in_history =
        wmbus_status_meets_threshold(record.status, settings->memory_threshold);
    wmbus_rx_view_push_packet(rx_view, &record, store_in_history);
}
