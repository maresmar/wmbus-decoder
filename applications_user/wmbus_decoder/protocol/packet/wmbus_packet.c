#include "wmbus_packet.h"

#include <furi.h>
#include <string.h>

#include "wmbus_packet_decode.h"
#include "wmbus_packet_parser.h"
#include "wmbus_packet_security.h"

const char* wmbus_packet_status_str(WmBusStatus status) {
    switch(status) {
    case WmBusStatusDecodeFail:
        return "Decode fail";
    case WmBusStatusNotPlausible:
        return "Not plausible";
    case WmBusStatusFramingError:
        return "Framing error";
    case WmBusStatusCrcBad:
        return "CRC bad";
    case WmBusStatusWeakRssi:
        return "Weak RSSI";
    case WmBusStatusOk:
        return "OK";
    case WmBusStatusParsed:
        return "Parsed";
    default:
        return "--";
    }
}

const char* wmbus_packet_status_short_label(WmBusStatus status) {
    switch(status) {
    case WmBusStatusDecodeFail:
        return "Decode";
    case WmBusStatusNotPlausible:
        return "Plausible";
    case WmBusStatusFramingError:
        return "Framing";
    case WmBusStatusCrcBad:
        return "CRC";
    case WmBusStatusWeakRssi:
        return "Weak RSSI";
    case WmBusStatusOk:
        return "OK";
    case WmBusStatusParsed:
        return "Parsed";
    default:
        return "--";
    }
}

const char* wmbus_packet_csv_logging_str(WmBusCsvLogging logging) {
    switch(logging) {
    case WmBusCsvLoggingBasic:
        return "Basic";
    case WmBusCsvLoggingFull:
        return "Full";
    case WmBusCsvLoggingNone:
    default:
        return "None";
    }
}

bool wmbus_packet_process_capture(
    const WmBusCaptureFrame* capture,
    const WmBusCryptoKeyStore* key_store,
    WmBusPacketRecord* record) {
    if(!capture || !record) return false;

    memset(record, 0, sizeof(*record));
    record->mode = capture->mode;
    record->raw_len = (uint16_t)capture->raw_len;
    record->capture_len =
        (uint16_t)((capture->len > sizeof(record->capture_bytes)) ? sizeof(record->capture_bytes) :
                                                                capture->len);
    record->best_offset = -1;
    record->rssi = capture->rssi;
    record->rx_tick = furi_get_tick();
    record->strong_rssi = (capture->rssi >= -70);
    memcpy(record->capture_bytes, capture->data, record->capture_len);

    uint8_t normalized[256] = {0};
    WmBusPacketDecodeState decode = {0};
    if(!wmbus_packet_decode_capture(capture, record, normalized, sizeof(normalized), &decode)) {
        return false;
    }

    if(record->plausible && decode.frame && decode.frame_len > 0U) {
        wmbus_packet_store_frame(record, decode.frame, decode.frame_len);
        wmbus_packet_resolve_application_payload(decode.frame, decode.frame_len, record, key_store);
        wmbus_packet_parse_application(record);
        wmbus_packet_finalize_parser(record);
    } else {
        record->packet_is_frame = false;
        record->packet_len = (uint16_t)((capture->len > sizeof(record->packet_bytes)) ?
                                            sizeof(record->packet_bytes) :
                                            capture->len);
        memcpy(record->packet_bytes, capture->data, record->packet_len);
        record->application.parser_id = WmBusParserIdRaw;
    }

    if(decode.used_3of6 && !record->decoded_ok) {
        record->status = WmBusStatusDecodeFail;
    } else if(!record->plausible) {
        record->status = WmBusStatusNotPlausible;
    } else if(!record->length_ok) {
        record->status = WmBusStatusFramingError;
    } else if(record->crc_known && !record->crc_ok) {
        record->status = WmBusStatusCrcBad;
    } else if(record->application.record_count > 0U) {
        record->status = WmBusStatusParsed;
    } else if(!record->strong_rssi) {
        record->status = WmBusStatusWeakRssi;
    } else {
        record->status = WmBusStatusOk;
    }

    return true;
}
